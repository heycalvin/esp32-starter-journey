#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

#define BSP_LCD_H_RES   240
#define BSP_LCD_V_RES   280

esp_err_t bsp_display_init(esp_lcd_panel_io_handle_t *out_io, esp_lcd_panel_handle_t *out_panel);
esp_err_t bsp_touch_init(esp_lcd_touch_handle_t *out_touch);
void bsp_display_set_backlight(uint8_t brightness_pct);
void bsp_display_set_backlight_pwm(uint8_t pct);  // 0~100% PWM 无级调光
uint8_t bsp_display_get_backlight_pct(void);
