#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float temperature;   // 实时气温 (°C)
    float humidity;      // 实时湿度 (%)
    float distance_cm;   // 超声波测距 (cm)
    bool  pir_motion;    // 人体感应 (true=有人靠近)
    uint32_t free_heap;  // 系统可用内存 (Bytes)
} hub_sensors_data_t;

/**
 * @brief 初始化传感器子系统 (ADC, DHT11, HC-SR04, SR602)
 */
esp_err_t bsp_sensors_init(void);

/**
 * @brief 读取传感器融合环境数据
 */
esp_err_t bsp_sensors_read_all(hub_sensors_data_t *data);

#ifdef __cplusplus
}
#endif
