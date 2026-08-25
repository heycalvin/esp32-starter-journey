#pragma once
#include <stdbool.h>
#include "esp_err.h"

esp_err_t bsp_led_init(void);
void bsp_led_set(bool is_on);
void bsp_led_toggle(void);
bool bsp_led_get_state(void);
