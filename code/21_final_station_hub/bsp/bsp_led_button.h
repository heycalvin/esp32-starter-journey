#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bsp_led_button_init(void);
esp_err_t bsp_led_set(bool on);
esp_err_t bsp_led_toggle(void);
bool bsp_led_get(void);
bool bsp_button_is_pressed(void);

#ifdef __cplusplus
}
#endif
