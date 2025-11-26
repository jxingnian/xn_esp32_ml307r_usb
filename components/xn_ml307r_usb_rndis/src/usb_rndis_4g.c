/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file usb_rndis_4g.c
 * @brief USB RNDIS 4G模块组件实现
 *
 * 本组件实现了通过USB连接4G模块(如ML307)实现网络通信的功能
 * 主要功能：
 * 1. 初始化USB主机CDC驱动，识别并连接USB 4G模块
 * 2. 通过RNDIS协议与4G模块进行通信，获取网络连接
 * 3. 支持网络状态事件回调通知
 * 4. 可选的ping测试功能，监控网络连通性
 *
 * 应用场景：
 * - 物联网网关：为无WiFi环境下的设备提供4G网络接入
 * - 工业场景：远程监控、数据采集等需要稳定移动网络的场合
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "ping/ping_sock.h"
#include "iot_usbh_rndis.h"
#include "iot_eth.h"
#include "iot_eth_netif_glue.h"
#include "iot_usbh_cdc.h"
#include "usb_rndis_4g.h"

// 日志标签
static const char *TAG = "usb_rndis_4g";

// 全局变量
static usb_rndis_event_callback_t g_event_callback = NULL;
static void *g_user_data = NULL;
static esp_ping_handle_t g_ping_handle = NULL;
static bool g_ping_running = false;
static iot_eth_handle_t g_eth_handle = NULL;
static esp_netif_t *g_eth_netif = NULL;
static bool g_eth_started = false;
static TaskHandle_t g_retry_task_handle = NULL;

/**
 * @brief 以太网驱动启动重试任务
 * 
 * 参考例程实现：每1秒重试一次iot_eth_start()，直到成功
 * 这是非阻塞版本，使用FreeRTOS任务实现
 */
static void eth_retry_task(void *arg)
{
    ESP_LOGI(TAG, "启动重试任务：每1秒尝试启动以太网驱动");
    
    while (!g_eth_started && g_eth_handle != NULL) {
        esp_err_t ret = iot_eth_start(g_eth_handle);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ 以太网驱动启动成功");
            g_eth_started = true;
            break;
        } else if (ret == ESP_ERR_INVALID_STATE) {
            // 已经在运行了
            ESP_LOGI(TAG, "以太网驱动已在运行");
            g_eth_started = true;
            break;
        } else {
            ESP_LOGD(TAG, "重试启动失败: %s，1秒后重试", esp_err_to_name(ret));
        }
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // 等待1秒，与例程相同
    }
    
    ESP_LOGI(TAG, "重试任务结束");
    g_retry_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Ping成功回调函数
 *
 * 当ping请求成功收到响应时被调用，用于输出ping统计信息
 */
static void on_ping_success(esp_ping_handle_t hdl, void *args)
{
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    
    // 获取ping统计信息
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    
    ESP_LOGI(TAG, "Ping: %"PRIu32" bytes from %s icmp_seq=%u ttl=%u time=%"PRIu32" ms",
             recv_len, ipaddr_ntoa(&target_addr), seqno, ttl, elapsed_time);
}

/**
 * @brief Ping超时回调函数
 *
 * 当ping请求在指定时间内未收到响应时被调用
 */
static void on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    
    ESP_LOGW(TAG, "Ping timeout: %s icmp_seq=%u", ipaddr_ntoa(&target_addr), seqno);
}

/**
 * @brief IOT以太网事件处理函数
 *
 * 处理USB RNDIS设备（4G模块）的各种状态事件
 */
static void iot_event_handle(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id) {
    case IOT_ETH_EVENT_START:
        ESP_LOGI(TAG, "IOT_ETH_EVENT_START");
        break;
        
    case IOT_ETH_EVENT_STOP:
        ESP_LOGI(TAG, "IOT_ETH_EVENT_STOP");
        if (g_event_callback) {
            g_event_callback(USB_RNDIS_EVENT_DISCONNECTED, NULL, g_user_data);
        }
        break;
        
    case IOT_ETH_EVENT_CONNECTED:
        ESP_LOGI(TAG, "IOT_ETH_EVENT_CONNECTED - 4G设备已连接");
        if (g_event_callback) {
            g_event_callback(USB_RNDIS_EVENT_CONNECTED, NULL, g_user_data);
        }
        break;
        
    case IOT_ETH_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "IOT_ETH_EVENT_DISCONNECTED - 4G设备断开连接");
        if (g_event_callback) {
            g_event_callback(USB_RNDIS_EVENT_DISCONNECTED, NULL, g_user_data);
        }
        break;
        
    default:
        ESP_LOGI(TAG, "IOT_ETH_EVENT_UNKNOWN: %d", (int)event_id);
        break;
    }
}

/**
 * @brief IP事件处理函数
 *
 * 处理网络接口的IP地址获取事件
 */
static void ip_event_handle(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_ip_info_t *ip_info = &event->ip_info;
        
        ESP_LOGI(TAG, "获取IP地址成功:");
        ESP_LOGI(TAG, "  IP地址: " IPSTR, IP2STR(&ip_info->ip));
        ESP_LOGI(TAG, "  网关  : " IPSTR, IP2STR(&ip_info->gw));
        ESP_LOGI(TAG, "  子网掩码: " IPSTR, IP2STR(&ip_info->netmask));
        
        vTaskDelay(2000 / portTICK_PERIOD_MS);  // 等待2秒
        // 通知应用层
        if (g_event_callback) {
            g_event_callback(USB_RNDIS_EVENT_GOT_IP, ip_info, g_user_data);
        }
    }
}

/**
 * @brief 初始化USB RNDIS 4G网络
 */
esp_err_t usb_rndis_4g_init(const usb_rndis_config_t *config)
{
    usb_rndis_config_t cfg = (config == NULL)
                                 ? USB_RNDIS_4G_DEFAULT_CONFIG()
                                 : *config;

    // 保存回调函数和用户数据
    g_event_callback = cfg.event_callback;
    g_user_data      = cfg.user_data;

    ESP_LOGI(TAG, "========== 初始化USB RNDIS 4G网络 ==========");
    
    // 初始化网络栈和默认事件循环（容忍已初始化状态）
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop create failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ========== 第一步：注册事件处理函数 ========== */
    // 注册IOT以太网事件处理函数
    ESP_ERROR_CHECK(esp_event_handler_register(IOT_ETH_EVENT, ESP_EVENT_ANY_ID, iot_event_handle, NULL));
    
    // 注册IP事件处理函数
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, ip_event_handle, NULL));
    
    /* ========== 第二步：安装USB CDC主机驱动 ========== */
    usbh_cdc_driver_config_t cdc_config = {
        .task_stack_size = 1024 * 4,           // CDC任务堆栈大小：4KB
        .task_priority = 5,                     // CDC任务优先级
        .task_coreid = 0,                       // CDC任务运行在核心0
        .skip_init_usb_host_driver = false,     // 不跳过USB主机驱动初始化
    };
    
    ret = usbh_cdc_driver_install(&cdc_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB CDC驱动安装失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "USB CDC驱动安装成功");
    
    /* ========== 第三步：创建USB RNDIS驱动 ========== */
    iot_usbh_rndis_config_t rndis_cfg = {
        .auto_detect = true,                        // 启用自动检测
        .auto_detect_timeout = pdMS_TO_TICKS(1000), // 检测超时：1秒
    };
    
    iot_eth_driver_t *rndis_handle = NULL;
    ret = iot_eth_new_usb_rndis(&rndis_cfg, &rndis_handle);
    if (ret != ESP_OK || rndis_handle == NULL) {
        ESP_LOGE(TAG, "创建USB RNDIS驱动失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "USB RNDIS驱动创建成功");
    
    /* ========== 第四步：安装以太网驱动 ========== */
    iot_eth_config_t eth_cfg = {
        .driver = rndis_handle,
        .stack_input = NULL,
        .user_data = NULL,
    };
    
    ret = iot_eth_install(&eth_cfg, &g_eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "安装以太网驱动失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "以太网驱动安装成功");
    
    /* ========== 第五步：创建并绑定网络接口 ========== */
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    g_eth_netif = esp_netif_new(&netif_cfg);
    if (g_eth_netif == NULL) {
        ESP_LOGE(TAG, "创建网络接口失败");
        return ESP_FAIL;
    }
    
    iot_eth_netif_glue_handle_t glue = iot_eth_new_netif_glue(g_eth_handle);
    if (glue == NULL) {
        ESP_LOGE(TAG, "创建netif glue失败");
        return ESP_FAIL;
    }
    
    ret = esp_netif_attach(g_eth_netif, glue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "附加网络接口失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "网络接口绑定成功");
    
    /* ========== 第六步：启动以太网驱动 ========== */
    // 参考例程：doc/usb_rndis_4g_module/main/usb_rndis_4g_module.c:233-241
    // 例程使用while循环阻塞式重试，这里使用任务实现非阻塞重试
    ESP_LOGI(TAG, "正在启动以太网驱动...");
    ret = iot_eth_start(g_eth_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ 以太网驱动首次启动成功");
        g_eth_started = true;
    } else {
        ESP_LOGW(TAG, "首次启动失败: %s", esp_err_to_name(ret));
        ESP_LOGI(TAG, "启动后台重试任务（参考例程实现）");
        
        // 创建重试任务，每1秒重试一次（与例程相同）
        BaseType_t task_ret = xTaskCreate(
            eth_retry_task,
            "eth_retry",
            4096,
            NULL,
            5,
            &g_retry_task_handle
        );
        
        if (task_ret != pdPASS) {
            ESP_LOGE(TAG, "创建重试任务失败");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "重试任务已启动，每1秒尝试启动以太网驱动");
    }
    
    ESP_LOGI(TAG, "========== USB RNDIS 4G网络初始化完成 ==========");
    ESP_LOGI(TAG, "等待4G设备连接...");
    
    return ESP_OK;
}

/**
 * @brief 启动网络连通性测试
 */
esp_err_t usb_rndis_4g_start_ping_test(void)
{
    if (g_ping_running) {
        ESP_LOGW(TAG, "Ping测试已在运行");
        return ESP_OK;
    }
    
    // 配置ping参数
    ip_addr_t target_addr;
    ipaddr_aton("124.237.177.164", &target_addr);  // Google DNS
    
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;
    ping_config.timeout_ms = 2000;
    ping_config.task_stack_size = 4096;
    ping_config.count = ESP_PING_COUNT_INFINITE;  // 持续ping
    ping_config.interval_ms = 1000;  // 5秒间隔
    
    // 设置回调函数
    esp_ping_callbacks_t cbs = {
        .on_ping_success = on_ping_success,
        .on_ping_timeout = on_ping_timeout,
        .on_ping_end = NULL,
        .cb_args = NULL,
    };
    
    // 创建ping会话
    esp_err_t ret = esp_ping_new_session(&ping_config, &cbs, &g_ping_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建ping会话失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 启动ping
    ret = esp_ping_start(g_ping_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动ping失败: %s", esp_err_to_name(ret));
        esp_ping_delete_session(g_ping_handle);
        g_ping_handle = NULL;
        return ret;
    }
    
    g_ping_running = true;
    ESP_LOGI(TAG, "Ping测试已启动 (目标: 8.8.8.8)");
    
    return ESP_OK;
}

/**
 * @brief 停止网络连通性测试
 */
esp_err_t usb_rndis_4g_stop_ping_test(void)
{
    if (!g_ping_running || g_ping_handle == NULL) {
        ESP_LOGW(TAG, "Ping测试未运行");
        return ESP_OK;
    }
    
    esp_err_t ret = esp_ping_stop(g_ping_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "停止ping失败: %s", esp_err_to_name(ret));
    }
    
    ret = esp_ping_delete_session(g_ping_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "删除ping会话失败: %s", esp_err_to_name(ret));
    }
    
    g_ping_handle = NULL;
    g_ping_running = false;
    
    ESP_LOGI(TAG, "Ping测试已停止");
    
    return ESP_OK;
}

