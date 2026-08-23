#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化用户按键硬件外设
 */
esp_err_t bsp_button_init(void);

/**
 * @brief 查询按键是否被按下（已消抖）
 */
bool bsp_button_is_pressed(void);

#ifdef __cplusplus
}
#endif
