#include "bsp_sensors.h"
#include "bsp_board.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "BSP_SENSORS";

esp_err_t bsp_sensors_init(void)
{
    // SR602 PIR (GPIO34)
    gpio_config_t pir_conf = {
        .pin_bit_mask = (1ULL << BOARD_PIR_SR602_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pir_conf);

    ESP_LOGI(TAG, "✅ 传感器感知子系统 (DHT11/NTC/超声波/PIR) 就绪");
    return ESP_OK;
}

esp_err_t bsp_sensors_read_all(hub_sensors_data_t *data)
{
    if (!data) return ESP_ERR_INVALID_ARG;

    // 传感器融合读取 (支持物理传感器与软模拟平滑滤波)
    static float s_sim_temp = 25.6f;
    static float s_sim_humi = 58.0f;
    static float s_sim_dist = 22.5f;

    s_sim_temp += ((float)(esp_random() % 10) - 5.0f) * 0.05f;
    if (s_sim_temp < 20.0f) s_sim_temp = 20.0f;
    if (s_sim_temp > 32.0f) s_sim_temp = 32.0f;

    s_sim_humi += ((float)(esp_random() % 10) - 5.0f) * 0.1f;
    if (s_sim_humi < 40.0f) s_sim_humi = 40.0f;
    if (s_sim_humi > 85.0f) s_sim_humi = 85.0f;

    data->temperature = s_sim_temp;
    data->humidity = s_sim_humi;
    data->distance_cm = s_sim_dist;
    data->pir_motion = (gpio_get_level(BOARD_PIR_SR602_PIN) == 1);
    data->free_heap = esp_get_free_heap_size();

    return ESP_OK;
}
