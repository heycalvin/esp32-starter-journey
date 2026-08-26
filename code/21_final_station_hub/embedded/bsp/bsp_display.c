#include "bsp_display.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_log.h"

static const char *TAG = "BSP_DISP";

#define LCD_HOST                SPI2_HOST
#define LCD_PIN_MOSI            GPIO_NUM_19
#define LCD_PIN_SCLK            GPIO_NUM_18
#define LCD_PIN_CS              GPIO_NUM_5
#define LCD_PIN_DC              GPIO_NUM_17
#define LCD_PIN_RST             GPIO_NUM_21
#define LCD_PIN_BACKLIGHT       GPIO_NUM_26

#define TOUCH_I2C_SCL           GPIO_NUM_22
#define TOUCH_I2C_SDA           GPIO_NUM_23
#define TOUCH_I2C_INT           GPIO_NUM_35

// 背光 LEDC PWM 配置 (LEDC_CHANNEL_1 / LEDC_TIMER_1，避免与 WS2812 CHANNEL_0 冲突)
#define BL_LEDC_TIMER           LEDC_TIMER_1
#define BL_LEDC_MODE            LEDC_LOW_SPEED_MODE
#define BL_LEDC_CHANNEL         LEDC_CHANNEL_1
#define BL_LEDC_DUTY_RES        LEDC_TIMER_8_BIT  // 8bit: 0~255
#define BL_LEDC_FREQUENCY       5000              // 5kHz，远高于视觉频闪阈值

static uint8_t s_backlight_pct = 100;

static void backlight_pwm_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = BL_LEDC_MODE,
        .timer_num        = BL_LEDC_TIMER,
        .duty_resolution  = BL_LEDC_DUTY_RES,
        .freq_hz          = BL_LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = BL_LEDC_MODE,
        .channel        = BL_LEDC_CHANNEL,
        .timer_sel      = BL_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LCD_PIN_BACKLIGHT,
        .duty           = 255,   // 初始满亮度
        .hpoint         = 0,
    };
    ledc_channel_config(&ledc_channel);
    ledc_fade_func_install(0);
}

esp_err_t bsp_display_init(esp_lcd_panel_io_handle_t *out_io, esp_lcd_panel_handle_t *out_panel)
{
    // 1. 初始化背光 LEDC PWM 通道（替代原 gpio_set_level，支持 0~100% 无级调光）
    backlight_pwm_init();

    // 2. 初始化 SPI 总线
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_SCLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = BSP_LCD_H_RES * 40 * sizeof(uint16_t),
    };
    esp_err_t ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 3. 配置 Panel IO
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io_handle));

    // 4. 配置 ST7789 Panel
    esp_lcd_panel_dev_config_t p_cfg = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &p_cfg, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    // 固定铁律：屏幕排线偏移与镜像配置，绝不修改！
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 20));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    if (out_io) *out_io = io_handle;
    if (out_panel) *out_panel = panel_handle;

    ESP_LOGI(TAG, "📺 [BSP] 1.69寸 ST7789 彩屏初始化完毕 (240x280, Gap 0,20)！");
    return ESP_OK;
}

esp_err_t bsp_touch_init(esp_lcd_touch_handle_t *out_touch)
{
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = TOUCH_I2C_SDA,
        .scl_io_num = TOUCH_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus = NULL;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus));

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_config, &tp_io_handle));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_H_RES,
        .y_max = BSP_LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = TOUCH_I2C_INT,
        .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
    };
    esp_lcd_touch_handle_t touch_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &touch_handle));

    if (out_touch) *out_touch = touch_handle;
    ESP_LOGI(TAG, "👆 [BSP] CST816S 电容触摸屏初始化完毕 (I2C 0x15)！");
    return ESP_OK;
}

/**
 * @brief 设置屏幕背光（兼容旧接口，内部调用 PWM）
 * @param brightness_pct 0~100
 */
void bsp_display_set_backlight(uint8_t brightness_pct)
{
    bsp_display_set_backlight_pwm(brightness_pct);
}

/**
 * @brief 设置屏幕背光 PWM 亮度（0~100%，5kHz LEDC，GPIO26）
 */
void bsp_display_set_backlight_pwm(uint8_t pct)
{
    if (pct > 100) pct = 100;
    s_backlight_pct = pct;
    // 将百分比映射到 8bit 占空比 (0~255)
    // 注意：亮度低于 10% 时保持最低亮度 10 避免屏幕完全熄灭（保留可见性）
    uint32_t duty = (pct == 0) ? 0 : (uint32_t)((pct * 255 / 100));
    if (duty < 25 && duty > 0) duty = 25; // 最低约 10% 亮度
    ledc_set_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL, duty);
    ledc_update_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL);
}

/**
 * @brief 获取当前背光亮度百分比
 */
uint8_t bsp_display_get_backlight_pct(void)
{
    return s_backlight_pct;
}
