<!--
 * @Author: 星年 && jixingnian@gmail.com
 * @Date: 2025-11-24 10:02:45
 * @LastEditors: xingnian jixingnian@gmail.com
 * @LastEditTime: 2025-11-24 11:03:06
 * @FilePath: \xn_ml307r_usb\README.md
 * @Description: 
 * 
 * Copyright (c) 2025 by ${git_name_email}, All Rights Reserved. 
-->

# xn_ml307r_usb

ESP32 通过 USB RNDIS 连接 4G 模块（如 ML307），将 4G 模块作为以太网网口使用，实现有线网络不可用或无 Wi‑Fi 环境下的 4G 联网能力。

本项目基于 ESP-IDF，核心功能由自定义组件 `usb_rndis_4g` 提供，`main` 只负责基础初始化与事件处理，方便在实际工程中直接复用。

## 功能概览

- 通过 USB Host 自动识别并连接支持 RNDIS 的 4G 模块；
- 基于 `espressif/iot_usbh_rndis` + `espressif/iot_eth` 封装 USB / RNDIS / 以太网驱动；
- 创建并绑定 `esp_netif` 以太网接口，对上层提供标准 TCP/IP 能力；
- 通过事件回调上报 4G 状态：
  - 设备已连接 / 已断开；
  - 已获取 IP 地址（附带 `esp_netif_ip_info_t`）；
- 可选的 Ping 连通性测试（默认周期性 Ping `8.8.8.8`），用于简单监控 4G 网络质量；
- 应用层通过 Socket / HTTP 客户端等 API 像使用普通以太网一样上网。

## 目录结构

- `main/`
  - `main.c`：应用入口，初始化 `esp_netif` 和默认事件循环，注册 4G 事件回调，调用 `usb_rndis_4g_init()`，在获得 IP 后可选择启动 Ping 测试。
- `components/usb_rndis_4g/`
  - `src/usb_rndis_4g.c`：USB RNDIS 4G 组件实现。
  - `include/usb_rndis_4g.h`：对外暴露的 API 与事件类型。
  - `idf_component.yml`：组件依赖声明（`iot_usbh_rndis`、`iot_eth` 等）。
  - `README.md`：组件级使用说明。

## 环境与依赖

- ESP-IDF 版本：`>= 4.4.0`（具体以 `idf_component.yml` 为准）；
- 支持 USB OTG/Host 的 ESP32 系列开发板；
- 支持 RNDIS 模式的 4G USB 模块（示例：ML307）；
- 正确连接的 USB 数据线与稳定的电源供给（4G 模块功耗较高）。

## 快速开始

1. 安装并配置 ESP-IDF 开发环境；
2. 在工程根目录执行：
   - `idf.py set-target <你的芯片型号>`
   - 根据需要运行 `idf.py menuconfig`，检查 USB Host、日志级别等配置；
3. 将 ML307 等 4G 模块通过 USB 连接到开发板；
4. 运行：
   - `idf.py build flash monitor`
5. 观察串口日志：
   - 查看 4G 设备连接状态与 IP 获取情况；
   - 如启用 Ping 测试，可看到到 `8.8.8.8` 的往返时间与丢包情况。

## 运行流程简介

1. `app_main` 初始化 `esp_netif` 和默认事件循环；
2. 注册 4G 事件回调函数（打印“已连接/已断开/已获取 IP”等日志）；
3. 调用 `usb_rndis_4g_init()`：
   - 安装 USB CDC 主机驱动；
   - 创建 USB RNDIS 驱动和以太网驱动；
   - 创建并绑定 `esp_netif` 以太网接口；
   - 启动以太网驱动，必要时通过后台任务重试；
4. 当收到 `USB_RNDIS_EVENT_GOT_IP` 事件时，4G 网络已就绪，可直接进行 TCP/IP 通信；
5. （可选）调用 `usb_rndis_4g_start_ping_test()` 启动 Ping 连通性测试，用于简单监控链路质量。

## 注意事项

- **电源与硬件连接**：确保 4G 模块供电充足，避免因电压跌落导致频繁掉线；
- **事件中避免长阻塞**：在事件回调中尽量只做日志与消息分发，耗时操作放到独立任务中；
- **Ping 测试仅用于调试**：量产场景可根据需要关闭或修改实现，以减少流量和功耗；
- **日志级别**：本项目默认日志较详细，便于开发调试，如需精简可在 `menuconfig` 中调整对应 TAG 的日志等级。