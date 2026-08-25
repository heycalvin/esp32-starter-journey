#include "bsp_led.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "BSP_LED";
#define BOARD_LED2_PIN  GPIO_NUM_27

static bool s_led_state = false;

esp_err_t bsp_led_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOARD_LED2_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret == ESP_OK) {
        gpio_set_level(BOARD_LED2_PIN, 0);
        s_led_state = false;
        ESP_LOGI(TAG, "🟢 [BSP] 板载 LED2 (GPIO%d) 初始化就绪", BOARD_LED2_PIN);
    }
    return ret;
}

void bsp_led_set(bool is_on)
{
    s_led_state = is_on;
    gpio_set_level(BOARD_LED2_PIN, is_on ? 1 : 0);
}

void bsp_led_toggle(void)
{
    bsp_led_set(!s_led_state);
}

bool bsp_led_get_state(void)
{
    return s_led_state;
}
