#include "srv_ble.h"
#include "esp_log.h"

static const char *TAG = "SRV_BLE";

esp_err_t srv_ble_start(const char *device_name)
{
    ESP_LOGI(TAG, "📡 [BLE 服务] 蓝牙近场中控服务启动，广播名: %s", device_name);
    return ESP_OK;
}
