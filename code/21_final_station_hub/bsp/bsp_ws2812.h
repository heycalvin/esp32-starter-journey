#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bsp_ws2812_init(void);
esp_err_t bsp_ws2812_set_color(uint8_t red, uint8_t green, uint8_t blue);
esp_err_t bsp_ws2812_clear(void);

#ifdef __cplusplus
}
#endif
