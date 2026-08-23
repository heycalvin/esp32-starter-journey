#include "bsp_led_button.h"
#include "bsp_board.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "BSP_LED_BTN";
static bool s_led_state = false;

esp_err_t bsp_led_button_init(void)
{
    // LED2
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << BOARD_LED2_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_conf);
    gpio_set_level(BOARD_LED2_PIN, 0);

    // SW3 Button
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BOARD_SW3_BTN_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_conf);

    ESP_LOGI(TAG, "✅ 板载 LED2 (GPIO%d) 与 SW3 按键 (GPIO%d) 初始化完成", BOARD_LED2_PIN, BOARD_SW3_BTN_PIN);
    return ESP_OK;
}

esp_err_t bsp_led_set(bool on)
{
    s_led_state = on;
    return gpio_set_level(BOARD_LED2_PIN, on ? 1 : 0);
}

esp_err_t bsp_led_toggle(void)
{
    return bsp_led_set(!s_led_state);
}

bool bsp_led_get(void)
{
    return s_led_state;
}

bool bsp_button_is_pressed(void)
{
    if (gpio_get_level(BOARD_SW3_BTN_PIN) == 0) {
        vTaskDelay(pdMS_TO_TICKS(20));
        return (gpio_get_level(BOARD_SW3_BTN_PIN) == 0);
    }
    return false;
}
