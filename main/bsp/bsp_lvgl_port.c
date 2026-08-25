#include "bsp_lvgl_port.h"
#include <assert.h>
#include "esp_lvgl_port.h"
#include "bsp_display.h"
#include "esp_log.h"

static const char *TAG = "BSP_LVGL";
static lv_display_t *s_lvgl_disp = NULL;

esp_err_t bsp_lvgl_port_init(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t panel, esp_lcd_touch_handle_t touch)
{
    // 1. 初始化 LVGL 端口
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    // 2. 注册显示设备 (双缓冲 + DMA)
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = BSP_LCD_H_RES * 40,
        .double_buffer = true,
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
        .monochrome = false,
        .rotation = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
        .flags = { .buff_dma = true, .swap_bytes = true }
    };
    s_lvgl_disp = lvgl_port_add_disp(&disp_cfg);
    assert(s_lvgl_disp != NULL);

    // 3. 注册触摸设备
    if (touch && s_lvgl_disp) {
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = s_lvgl_disp,
            .handle = touch,
        };
        lvgl_port_add_touch(&touch_cfg);
    }

    ESP_LOGI(TAG, "🎨 [BSP] LVGL v9 端口初始化就绪 (PSRAM 双缓冲 DMA 加速)！");
    return ESP_OK;
}

bool bsp_lvgl_port_lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void bsp_lvgl_port_unlock(void)
{
    lvgl_port_unlock();
}
