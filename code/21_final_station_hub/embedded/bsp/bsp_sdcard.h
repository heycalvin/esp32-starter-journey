#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t bsp_sdcard_init(void);
bool bsp_sdcard_is_mounted(void);
esp_err_t bsp_sdcard_get_space_mb(uint32_t *total_mb, uint32_t *free_mb);
