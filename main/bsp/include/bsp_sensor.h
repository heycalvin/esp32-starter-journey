#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float temperature;
    float humidity;
    uint32_t free_heap;
} bsp_sensor_data_t;

/**
 * @brief 初始化传感器子系统
 */
esp_err_t bsp_sensor_init(void);

/**
 * @brief 采集环境与系统健康数据
 */
esp_err_t bsp_sensor_read(bsp_sensor_data_t *data);

#ifdef __cplusplus
}
#endif
