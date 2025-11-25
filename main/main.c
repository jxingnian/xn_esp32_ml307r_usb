#include "esp_log.h"
#include "at_modem.h"

static const char *TAG = "ML307_DEMO";

void TestHttp(std::unique_ptr<AtModem>& modem) {
    ESP_LOGI(TAG, "开始 HTTP 测试");

    // 创建 HTTP 客户端
    auto http = modem->CreateHttp(0);
    
    // 设置请求头
    http->SetHeader("User-Agent", "Xiaozhi/3.0.0");
    http->SetTimeout(10000);
    
    // 发送 GET 请求
    if (http->Open("GET", "https://httpbin.org/json")) {
        ESP_LOGI(TAG, "HTTP 状态码: %d", http->GetStatusCode());
        ESP_LOGI(TAG, "响应内容长度: %zu bytes", http->GetBodyLength());
        
        // 读取响应内容
        std::string response = http->ReadAll();
        ESP_LOGI(TAG, "响应内容: %s", response.c_str());
        
        http->Close();
    } else {
        ESP_LOGE(TAG, "HTTP 请求失败");
    }
    
    // unique_ptr 会自动释放内存，无需手动 delete
}

extern "C" void app_main(void) {
    // 自动检测并初始化模组
    auto modem = AtModem::Detect(GPIO_NUM_13, GPIO_NUM_14, GPIO_NUM_15, 921600);
    
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
}