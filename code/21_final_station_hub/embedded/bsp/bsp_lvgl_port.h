#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"

esp_err_t bsp_lvgl_port_init(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t panel, esp_lcd_touch_handle_t touch);
bool bsp_lvgl_port_lock(uint32_t timeout_ms);
void bsp_lvgl_port_unlock(void);
