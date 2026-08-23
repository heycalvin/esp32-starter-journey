/**
 * 🌟 ESP32 物联网实战 —— 第 10 关 基础实验：ST7789 (0,0) 原点与红点方向标定测试
 * 
 * 🎯 学习目标：
 *    1. 在屏幕物理坐标原点 (0, 0) 处绘制一个 30x30 像素的鲜艳红色实心方块；
 *    2. 掌握“红点试探法”快速判断屏幕旋转角度（0°/90°/180°/270°）与 X/Y 轴正方向；
 *    3. 验证 ST7789 物理视口偏移量（Gap: X=20, Y=0）是否正确对齐。
 * 
 * 📌 硬件引脚分配：
 *    - SPI SCLK:      GPIO18
 *    - SPI MOSI:      GPIO19
 *    - LCD CS:        GPIO5
 *    - LCD DC:        GPIO17
 *    - LCD RST:       GPIO21
 *    - LCD Backlight: GPIO26 (背光引脚)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"

static const char *TAG = "EXP0_ORIGIN_CALIBRATE";

#define LCD_HOST                SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ      (20 * 1000 * 1000) // 20MHz 稳定时钟
#define LCD_PIN_MOSI            GPIO_NUM_19
#define LCD_PIN_SCLK            GPIO_NUM_18
#define LCD_PIN_CS              GPIO_NUM_5
#define LCD_PIN_DC              GPIO_NUM_17
#define LCD_PIN_RST             GPIO_NUM_21
#define LCD_PIN_BACKLIGHT       GPIO_NUM_26

#define LCD_H_RES               240
#define LCD_V_RES               280
#define LCD_GAP_X               0   // ST7789 240x280 物理视口 X 偏移
#define LCD_GAP_Y               20  // ST7789 240x280 物理视口 Y 偏移

// RGB565 常用色彩定义 (小端字节序)
#define COLOR_BLACK             0x0000
#define COLOR_WHITE             0xFFFF
#define COLOR_RED               0x00F8  // 鲜艳纯红 (RGB565 0xF800 小端)
#define COLOR_GREEN             0xE007
#define COLOR_BLUE              0x1F00
#define COLOR_YELLOW            0xE0FF

static esp_lcd_panel_handle_t s_panel = NULL;

static void lcd_backlight_init(void)
{
    gpio_config_t bl_conf = {
        .pin_bit_mask = (1ULL << LCD_PIN_BACKLIGHT),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bl_conf);
    gpio_set_level(LCD_PIN_BACKLIGHT, 1); // 点亮背光
}

static void lcd_init(void)
{
    lcd_backlight_init();

    // 1. 初始化 SPI 总线 (开启 DMA 自动通道)
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_SCLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 2. 挂载 ST7789 Panel IO
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    // 3. 初始化 ST7789 驱动
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_panel, LCD_GAP_X, LCD_GAP_Y));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    ESP_LOGI(TAG, "✅ 1.69寸 ST7789 彩屏初始化成功 (分辨率: 240x280, Gap: X=%d, Y=%d)", LCD_GAP_X, LCD_GAP_Y);
}

/* 全屏纯色填充 */
static void fill_screen(uint16_t color)
{
    uint16_t *line_buf = heap_caps_malloc(LCD_H_RES * 20 * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!line_buf) return;

    for (int i = 0; i < LCD_H_RES * 20; i++) {
        line_buf[i] = color;
    }

    for (int y = 0; y < LCD_V_RES; y += 20) {
        esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_H_RES, y + 20, line_buf);
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    free(line_buf);
}

/* 填充指定矩形区域 (例如在 (0,0) 画 30x30 方块) */
static void fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (x >= LCD_H_RES || y >= LCD_V_RES || w <= 0 || h <= 0) return;
    if (x + w > LCD_H_RES) w = LCD_H_RES - x;
    if (y + h > LCD_V_RES) h = LCD_V_RES - y;

    if (x == 0 && y == 0 && w == LCD_H_RES && h == LCD_V_RES) {
        fill_screen(color);
        return;
    }

    int total_pixels = w * h;
    uint16_t *buf = heap_caps_malloc(total_pixels * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf) {
        uint16_t *line = heap_caps_malloc(w * sizeof(uint16_t), MALLOC_CAP_DMA);
        if (!line) return;
        for (int i = 0; i < w; i++) line[i] = color;
        for (int row = y; row < y + h; row++) {
            esp_lcd_panel_draw_bitmap(s_panel, x, row, x + w, row + 1, line);
        }
        free(line);
        return;
    }

    for (int i = 0; i < total_pixels; i++) {
        buf[i] = color;
    }

    esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + h, buf);
    free(buf);
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🟥 ST7789 (0,0) 原点 30x30 红色方块标定测试    ");
    ESP_LOGI(TAG, "==================================================");

    lcd_init();

    // 1. 全屏刷黑底
    ESP_LOGI(TAG, "🎨 1. 全屏清屏为纯黑色 (COLOR_BLACK)...");
    fill_screen(COLOR_BLACK);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 2. 在 (0, 0) 原点位置画一个 30x30 像素的红色实心方块
    ESP_LOGI(TAG, "🔴 2. 在 (0,0) 位置绘制 30x30 红色方块: fill_rect(0, 0, 30, 30, COLOR_RED)");
    fill_rect(0, 0, 30, 30, COLOR_RED);

    ESP_LOGI(TAG, "✅ 绘制完成！请观察屏幕左上角是否出现 30x30 红色方块。");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
