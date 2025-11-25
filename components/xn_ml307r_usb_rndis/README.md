## usb_rndis_4g 组件说明

### 简介

`usb_rndis_4g` 是基于 ESP-IDF 的 USB RNDIS 4G 上网组件，用于在 ESP32 系列芯片上通过 USB Host 连接 4G 模块（如 ML307），并将其作为以太网网口使用，为应用提供 TCP/IP 网络能力。

组件内部封装了 USB RNDIS 驱动、以太网抽象层和 `esp_netif` 网络接口创建与绑定等细节，应用层只需调用少量 API 并注册事件回调，即可在无 Wi‑Fi 场景下快速接入 4G 网络。

### 功能特性

- **USB RNDIS 4G 接入**：
  - 通过 USB Host 自动检测并连接支持 RNDIS 的 4G 模块；
  - 使用 `espressif/iot_usbh_rndis` 创建 RNDIS 以太网驱动。

- **以太网栈集成**：
  - 基于 `espressif/iot_eth` 安装以太网驱动；
  - 创建 `esp_netif` 以太网接口并与驱动绑定；
  - 启动以太网驱动，带后台重试任务，提升连接成功率。

- **事件回调机制**：
  - 通过应用注册的回调函数上报：
    - 4G 设备已连接 (`USB_RNDIS_EVENT_CONNECTED`)
    - 4G 设备已断开 (`USB_RNDIS_EVENT_DISCONNECTED`)
    - 已获取 IP 地址 (`USB_RNDIS_EVENT_GOT_IP`)，并可携带 `esp_netif_ip_info_t` 信息。

- **可选 Ping 连通性测试**：
  - 内置基于 `esp_ping` 的长周期 Ping 测试；
  - 默认目标为 `8.8.8.8`，用于简单监控 4G 网络连通性；
  - 支持随时启动与停止 Ping 会话。

### 组件依赖

依赖通过 `idf_component.yml` 声明，主要包括：

- ESP-IDF：`>= 4.4.0`
- USB RNDIS 驱动：`espressif/iot_usbh_rndis ^0.2.0`
- 以太网抽象层：`espressif/iot_eth ^0.1.0`

请确保工程已启用 USB Host 相关配置，并使用支持 USB OTG/Host 的芯片与开发板。

### API 概览

具体函数与类型请参考头文件 `usb_rndis_4g.h`，典型接口包括：

- **初始化 4G RNDIS 网络**：
  - `esp_err_t usb_rndis_4g_init(const usb_rndis_config_t *config);`
  - 完成事件注册、USB CDC 驱动安装、RNDIS 驱动创建、以太网驱动安装、netif 创建与绑定，以及以太网启动与重试逻辑。

- **事件类型与回调**：
  - `usb_rndis_event_t`：4G 状态事件（连接、断开、获取 IP 等）；
  - `usb_rndis_event_callback_t`：应用层事件回调函数类型，参数中可携带 `esp_netif_ip_info_t`。

- **网络连通性测试**：
  - `esp_err_t usb_rndis_4g_start_ping_test(void);`
  - `esp_err_t usb_rndis_4g_stop_ping_test(void);`
  - 启动/停止一个后台 Ping 会话，周期性向固定目标地址发送 Ping，并在日志中输出结果。

### 基本使用流程（文字示意）

1. **初始化网络栈和事件循环**：
   - 调用 `esp_netif_init()`；
   - 调用 `esp_event_loop_create_default()`（或复用已有默认事件循环）。

2. **实现并注册 4G 事件回调函数**：
   - 根据 `USB_RNDIS_EVENT_*` 事件输出日志或触发业务逻辑；
   - 在 `USB_RNDIS_EVENT_GOT_IP` 事件中读取 `esp_netif_ip_info_t`，保存 IP 信息用于后续访问。

3. **调用 `usb_rndis_4g_init()` 完成组件初始化**：
   - 传入包含事件回调、用户数据等的配置结构体；
   - 初始化成功后，等待 4G 设备通过 USB 接入并自动建立 RNDIS 网络。

4. **（可选）启动 Ping 测试**：
   - 在获取 IP 后调用 `usb_rndis_4g_start_ping_test()`；
   - 通过日志观察 4G 网络连通性；
   - 需要结束时调用 `usb_rndis_4g_stop_ping_test()`。

5. **在应用中使用网络**：
   - 通过 BSD Socket、HTTP 客户端等 API，像使用普通以太网接口一样访问外网或服务器。

### 注意事项

- **USB 供电与硬件连接**：
  - 4G 模块功耗较大，请确保 USB 口供电能力充足；
  - 确认 USB 数据线连接正确，ESP32 端已启用 USB Host 模式。

- **任务与栈空间**：
  - 组件内部会创建 USB CDC 任务与以太网重试任务，可根据实际需求调整任务栈大小与优先级；
  - 在事件回调中避免执行耗时操作，建议只做状态记录和消息分发。

- **Ping 测试仅作简单监控**：
  - 长时间持续 Ping 会占用一定带宽与功耗，量产环境可根据需要选择开启或修改实现。

- **日志级别**：
  - 默认输出较详细的日志便于调试，如需减少日志，可在工程中调整对应 TAG 的日志级别。

