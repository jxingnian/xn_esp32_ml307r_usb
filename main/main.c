/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-11-22 13:43:50
 * @LastEditors: xingnian jixingnian@gmail.com
 * @LastEditTime: 2025-11-24 11:14:54
 * @FilePath: \xn_ml307r_usb\main\main.c
 * @Description: esp32 网页WiFi配网 By.星年
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "usb_rndis_4g.h"

// 4G 事件回调：通过日志输出当前状态
static void usb_4g_event_handler(usb_rndis_event_t event,
                                 esp_netif_ip_info_t *ip_info,
                                 void *user_data)
{
    (void)user_data;

    switch (event) {
    case USB_RNDIS_EVENT_CONNECTED:
        ESP_LOGI("app", "4G 设备已连接");
        break;
    case USB_RNDIS_EVENT_DISCONNECTED:
        ESP_LOGW("app", "4G 设备已断开");
        break;
    case USB_RNDIS_EVENT_GOT_IP:
        if (ip_info) {
            ESP_LOGI("app", "4G 已获取 IP: " IPSTR, IP2STR(&ip_info->ip));
        } else {
            ESP_LOGI("app", "4G 已获取 IP");
        }
        break;
    default:
        break;
    }
}

void app_main(void)
{
    printf("USB RNDIS 4G 联网示例 By.星年\n");

    // 初始化网络栈和默认事件循环（usb_rndis_4g 组件依赖）
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE("app", "esp_netif_init failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE("app", "event loop create failed: %s", esp_err_to_name(ret));
        return;
    }

    // 使用默认配置 + 自定义事件回调初始化 4G 模块
    usb_rndis_config_t cfg = USB_RNDIS_4G_DEFAULT_CONFIG();
    cfg.event_callback     = usb_4g_event_handler;
    cfg.user_data          = NULL;

    ret = usb_rndis_4g_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE("app", "usb_rndis_4g_init failed: %s", esp_err_to_name(ret));
        return;
    }

    // 可选：启动 ping 测试，用于简单监控 4G 网络连通性
    (void)usb_rndis_4g_start_ping_test();

    // app_main 返回后，后续工作由事件回调和后台任务驱动
}
