#include "bsp_ws2812.h"
#include "bsp_board.h"
#include "esp_log.h"
#include "led_strip.h"

static const char *TAG = "BSP_WS2812";
static led_strip_handle_t s_led_strip = NULL;

esp_err_t bsp_ws2812_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = BOARD_WS2812_PIN,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip);
    if (ret == ESP_OK) {
        led_strip_clear(s_led_strip);
        ESP_LOGI(TAG, "✅ WS2812 幻彩 RGB (GPIO%d) 初始化成功", BOARD_WS2812_PIN);
    }
    return ret;
}

esp_err_t bsp_ws2812_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    if (!s_led_strip) return ESP_ERR_INVALID_STATE;
    led_strip_set_pixel(s_led_strip, 0, red, green, blue);
    return led_strip_refresh(s_led_strip);
}

esp_err_t bsp_ws2812_clear(void)
{
    if (!s_led_strip) return ESP_ERR_INVALID_STATE;
    return led_strip_clear(s_led_strip);
}
