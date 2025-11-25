/*
 * @Author: 星年 && jixingnian@gmail.com
 * @Date: 2025-11-25 20:18:39
 * @LastEditors: xingnian jixingnian@gmail.com
 * @LastEditTime: 2025-11-25 20:37:16
 * @FilePath: \xn_esp32_ml307r_usb\main\main.cpp
 * @Description: 
 * 
 * Copyright (c) 2025 by ${git_name_email}, All Rights Reserved. 
 */
#include <memory>
#include <string>

#include "esp_log.h"
#include "esp_timer.h"
#include "at_modem.h"

static const char *TAG = "ML307_DEMO";

void TestHttp(std::unique_ptr<AtModem>& modem) {
    ESP_LOGI(TAG, "开始 HTTP 测试");

    // 创建 HTTP 客户端
    auto http = modem->CreateHttp(0);
    
    // 设置请求头
    http->SetHeader("User-Agent", "Xiaozhi/3.0.0");
    http->SetTimeout(60000);

    const char *url = "http://win.xingnian.vip:16623/firmware/esp-chunfeng.bin";
    
    // 发送 GET 请求并统计下载时间与速度
    int64_t start_time_us = esp_timer_get_time();
    if (http->Open("GET", url)) {
        ESP_LOGI(TAG, "HTTP 状态码: %d", http->GetStatusCode());
        ESP_LOGI(TAG, "响应内容长度(服务器声明): %zu bytes", http->GetBodyLength());

        size_t downloaded_bytes = 0;
        constexpr size_t buffer_size = 4096;
        char buffer[buffer_size];

        while (true) {
            int read_len = http->Read(buffer, buffer_size);
            if (read_len < 0) {
                ESP_LOGE(TAG, "HTTP 读取失败");
                break;
            }
            if (read_len == 0) {
                break;
            }
            downloaded_bytes += static_cast<size_t>(read_len);
        }

        int64_t end_time_us = esp_timer_get_time();
        double elapsed_s = (end_time_us - start_time_us) / 1000000.0;

        if (elapsed_s > 0 && downloaded_bytes > 0) {
            double speed_kBps = downloaded_bytes / 1024.0 / elapsed_s;
            double speed_Mbps = downloaded_bytes * 8.0 / 1000.0 / 1000.0 / elapsed_s;
            ESP_LOGI(TAG, "实际下载大小: %zu bytes", downloaded_bytes);
            ESP_LOGI(TAG, "耗时: %.2f s", elapsed_s);
            ESP_LOGI(TAG, "平均速度: %.2f kB/s (%.2f Mbps)", speed_kBps, speed_Mbps);
        } else {
            ESP_LOGW(TAG, "下载耗时过短或未收到数据，无法计算速度");
        }
        
        http->Close();
    } else {
        ESP_LOGE(TAG, "HTTP 请求失败");
    }
    
    // unique_ptr 会自动释放内存，无需手动 delete
}

extern "C" void app_main(void) {
    // 自动检测并初始化模组
    auto modem = AtModem::Detect(GPIO_NUM_17, GPIO_NUM_13, GPIO_NUM_NC, 921600);
    
    if (!modem) {
        ESP_LOGE(TAG, "模组检测失败");
        return;
    }
    
    // 设置网络状态回调
    modem->OnNetworkStateChanged([](bool ready) {
        ESP_LOGI(TAG, "网络状态: %s", ready ? "已连接" : "已断开");
    });
    
    // 等待网络就绪
    NetworkStatus status = modem->WaitForNetworkReady(30000);
    if (status != NetworkStatus::Ready) {
        ESP_LOGE(TAG, "网络连接失败");
        return;
    }
    
    // 打印模组信息
    ESP_LOGI(TAG, "模组版本: %s", modem->GetModuleRevision().c_str());
    ESP_LOGI(TAG, "IMEI: %s", modem->GetImei().c_str());
    ESP_LOGI(TAG, "ICCID: %s", modem->GetIccid().c_str());
    ESP_LOGI(TAG, "运营商: %s", modem->GetCarrierName().c_str());
    ESP_LOGI(TAG, "信号强度: %d", modem->GetCsq());

    TestHttp(modem);
}
