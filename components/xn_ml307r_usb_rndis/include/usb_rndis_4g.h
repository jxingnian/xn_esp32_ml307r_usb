/*
 * @Author: 星年 && jixingnian@gmail.com
 * @Date: 2025-11-24 10:03:46
 * @LastEditors: xingnian jixingnian@gmail.com
 * @LastEditTime: 2025-11-24 11:08:21
 * @FilePath: \xn_ml307r_usb\components\usb_rndis_4g\include\usb_rndis_4g.h
 * @Description: USB RNDIS 4G 模块组件
 * 
 * Copyright (c) 2025 by ${git_name_email}, All Rights Reserved. 
 */

#pragma once  // 防止头文件被重复包含

#include "esp_err.h"      // ESP-IDF 错误码定义
#include "esp_netif.h"    // 网络接口与 IP 信息结构体定义

#ifdef __cplusplus
extern "C" {              // 兼容 C++ 编译
#endif

/**
 * @brief USB RNDIS 4G 事件类型
 */
typedef enum {
    USB_RNDIS_EVENT_CONNECTED,      ///< 4G 设备已物理连接（USB 枚举成功）
    USB_RNDIS_EVENT_DISCONNECTED,   ///< 4G 设备已断开（USB 断开或异常）
    USB_RNDIS_EVENT_GOT_IP,         ///< 已获取 IP 地址（网络就绪）
} usb_rndis_event_t;

/**
 * @brief USB RNDIS 事件回调函数类型
 *
 * @param event     当前发生的事件类型
 * @param ip_info   IP 信息指针，仅在 USB_RNDIS_EVENT_GOT_IP 时有效，其它事件为 NULL
 * @param user_data 用户自定义指针，由配置结构体透传
 */
typedef void (*usb_rndis_event_callback_t)(usb_rndis_event_t event,     ///< 事件类型
                                           esp_netif_ip_info_t *ip_info,///< IP 信息（仅 GOT_IP 时有效）
                                           void *user_data);            ///< 用户自定义数据

/**
 * @brief USB RNDIS 4G 初始化配置
 */
typedef struct {
    usb_rndis_event_callback_t event_callback;  ///< 事件回调函数指针，可为 NULL 表示不关心事件
    void *user_data;                            ///< 用户上下文指针，回调时原样返回
} usb_rndis_config_t;

/**
 * @brief USB RNDIS 4G 默认配置
 */
#define USB_RNDIS_4G_DEFAULT_CONFIG()   \
    (usb_rndis_config_t){               \
        .event_callback = NULL,         \
        .user_data      = NULL,         \
    }

/**
 * @brief 初始化 USB RNDIS 4G 网络（安装 USB/RNDIS/以太网驱动并开始监听 4G 设备）
 *
 * @param config 初始化配置指针，可为 NULL，NULL 时使用 USB_RNDIS_4G_DEFAULT_CONFIG()
 * @return ESP_OK              成功
 * @return ESP_ERR_INVALID_ARG 参数非法
 * @return ESP_ERR_NO_MEM      内存不足
 * @return 其他                底层驱动初始化失败
 *
 * @note 需在应用中先调用 esp_netif_init()、esp_event_loop_create_default() 等基础初始化接口
 */
esp_err_t usb_rndis_4g_init(const usb_rndis_config_t *config);  ///< 初始化 USB RNDIS 4G

/**
 * @brief 启动网络连通性测试（周期性 ping 8.8.8.8）
 *
 * @return ESP_OK  已成功启动或已在运行
 * @return 其他    创建或启动 ping 会话失败
 *
 * @note 仅作为链路质量监控工具，结果通过日志输出
 */
esp_err_t usb_rndis_4g_start_ping_test(void);   ///< 启动 ping 连通性测试

/**
 * @brief 停止网络连通性测试（销毁内部 ping 会话）
 *
 * @return ESP_OK  已成功停止或本就未运行
 * @return 其他    停止或删除会话时出错
 */
esp_err_t usb_rndis_4g_stop_ping_test(void);    ///< 停止 ping 连通性测试

#ifdef __cplusplus
}  // extern "C"
#endif
