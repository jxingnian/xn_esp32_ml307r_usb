# USB RNDIS 4G组件
# 提供USB RNDIS 4G网络连接功能

idf_component_register(
    SRCS 
        "src/usb_rndis_4g.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES 
        esp_netif
        esp_event
        lwip
        usb
        espressif__iot_usbh_rndis  
        espressif__iot_eth
    REQUIRES
        esp_eth
)
