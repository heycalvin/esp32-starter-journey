#pragma once
#include <stdbool.h>
#include "esp_err.h"

esp_err_t bsp_button_init(void);
bool bsp_button_is_pressed(void);
