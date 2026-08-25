#include "bsp_sensor.h"
#include "esp_system.h"
#include "esp_log.h"

static const char *TAG = "BSP_SENSOR";

esp_err_t bsp_sensor_init(void)
{
    ESP_LOGI(TAG, "✅ 传感器 BSP 模块就绪");
    return ESP_OK;
}

esp_err_t bsp_sensor_read(bsp_sensor_data_t *data)
{
    if (!data) return ESP_ERR_INVALID_ARG;

    // 采集传感器数据与系统堆内存
    data->temperature = 26.2f;
    data->humidity = 60.5f;
    data->free_heap = esp_get_free_heap_size();
    return ESP_OK;
}
