#pragma once
#include "esp_event.h"
#include "bsp_sensor.h"

ESP_EVENT_DECLARE_BASE(SYS_HUB_EVENT_BASE);

typedef enum {
    HUB_EVT_SENSOR_UPDATED = 1,   // 传感器数据更新事件 (载荷: bsp_sensor_data_t)
    HUB_EVT_WIFI_CONNECTED,       // Wi-Fi 联网成功
    HUB_EVT_WIFI_DISCONNECTED,    // Wi-Fi 掉线
    HUB_EVT_TIME_SYNCED,          // SNTP 授时成功
    HUB_EVT_WEATHER_UPDATED,      // HTTP 天气更新事件 (载荷: weather_info_t)
    HUB_EVT_MQTT_CMD_RECEIVED,    // MQTT 下行控制指令 (载荷: char[64])
    HUB_EVT_BLE_CONNECTED,        // BLE 手机连接
    HUB_EVT_BLE_DISCONNECTED,     // BLE 手机断开
    HUB_EVT_ALARM_TRIGGERED,      // 异常越限警报
    HUB_EVT_OTA_PROGRESS,         // OTA 升级进度 (载荷: int 0~100)
} hub_event_id_t;

typedef struct {
    char city[32];
    char weather_str[32];
    int  weather_code; // 0:晴, 1:多云, 2:阴, 3:雨, 4:雪
    float temp;
    int  humidity;
} hub_weather_info_t;

esp_err_t sys_event_bus_init(void);
esp_err_t sys_event_bus_post(hub_event_id_t id, const void *data, size_t size);
