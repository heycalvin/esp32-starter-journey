#include "bsp_led.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "BSP_LED";
#define BOARD_LED_PIN   GPIO_NUM_27

static bool s_led_is_on = false;

esp_err_t bsp_led_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOARD_LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret == ESP_OK) {
        gpio_set_level(BOARD_LED_PIN, 0);
        s_led_is_on = false;
        ESP_LOGI(TAG, "✅ 板载 LED2 初始化就绪 (GPIO%d)", BOARD_LED_PIN);
    }
    return ret;
}

esp_err_t bsp_led_set(bsp_led_state_t state)
{
    if (state == BSP_LED_TOGGLE) {
        s_led_is_on = !s_led_is_on;
    } else {
        s_led_is_on = (state == BSP_LED_ON);
    }
    return gpio_set_level(BOARD_LED_PIN, s_led_is_on ? 1 : 0);
}

bool bsp_led_get_state(void)
{
    return s_led_is_on;
}
