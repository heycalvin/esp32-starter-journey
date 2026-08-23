#include "bsp_button.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "BSP_BTN";
#define BOARD_BUTTON_PIN   GPIO_NUM_39

esp_err_t bsp_button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOARD_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ 板载 SW3 用户按键初始化就绪 (GPIO%d)", BOARD_BUTTON_PIN);
    }
    return ret;
}

bool bsp_button_is_pressed(void)
{
    if (gpio_get_level(BOARD_BUTTON_PIN) == 0) {
        vTaskDelay(pdMS_TO_TICKS(20)); // 软件消抖
        return (gpio_get_level(BOARD_BUTTON_PIN) == 0);
    }
    return false;
}
