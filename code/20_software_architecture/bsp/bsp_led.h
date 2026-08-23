#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BSP_LED_OFF = 0,
    BSP_LED_ON  = 1,
    BSP_LED_TOGGLE
} bsp_led_state_t;

/**
 * @brief 初始化板载 LED 硬件外设
 */
esp_err_t bsp_led_init(void);

/**
 * @brief 控制板载 LED 状态
 */
esp_err_t bsp_led_set(bsp_led_state_t state);

/**
 * @brief 获取当前 LED 开关状态
 */
bool bsp_led_get_state(void);

#ifdef __cplusplus
}
#endif
