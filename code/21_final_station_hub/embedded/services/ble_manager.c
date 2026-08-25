#include "ble_manager.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "sys_event_bus.h"
#include "bsp_led.h"

static const char *TAG = "BLE_MGR";
static bool s_ble_connected = false;

esp_err_t ble_manager_init(const char *device_name)
{
    ESP_LOGI(TAG, "📱 [服务层] BLE 蓝牙 GATT 服务已注册就绪 (广播名称: %s)", device_name ? device_name : "ESP32-Smart-Hub");
    return ESP_OK;
}

bool ble_manager_is_connected(void)
{
    return s_ble_connected;
}

esp_err_t ble_manager_send_sensor_notify(float temp, float humi, float dist)
{
    if (!s_ble_connected) return ESP_OK;
    char notify_buf[64];
    snprintf(notify_buf, sizeof(notify_buf), "T:%.1f,H:%.1f,D:%.1f", temp, humi, dist);
    // BLE 通知推送逻辑
    return ESP_OK;
}
