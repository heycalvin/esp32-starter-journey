#pragma once
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    float ntc_temperature;     // NTC 热敏电阻高精度温度 (°C)
    float dht_temperature;     // DHT11 温度 (°C)
    float dht_humidity;        // DHT11 湿度 (%)
    float ultrasonic_dist_cm;  // HC-SR04 超声波测距 (cm)
    bool  pir_motion_detected; // SR602 人体红外感应
    uint32_t free_heap_bytes;  // 系统可用堆内存 (Bytes)
    uint32_t free_psram_bytes; // 系统可用 PSRAM 内存 (Bytes)
} bsp_sensor_data_t;

esp_err_t bsp_sensor_init(void);
esp_err_t bsp_sensor_read_all(bsp_sensor_data_t *data);
float bsp_sensor_read_ntc_temp(void);
float bsp_sensor_read_ultrasonic_distance(void);
bool bsp_sensor_read_pir(void);
