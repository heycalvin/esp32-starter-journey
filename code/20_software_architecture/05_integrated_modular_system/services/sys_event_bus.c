#include "sys_event_bus.h"
#include "esp_log.h"

static const char *TAG = "SYS_EVENT_BUS";

ESP_EVENT_DEFINE_BASE(SYS_EVENT_BASE);

esp_err_t sys_event_bus_init(void)
{
    esp_err_t ret = esp_event_loop_create_default();
    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "✅ 统一事件总线 (Event Bus) 初始化成功");
        return ESP_OK;
    }
    return ret;
}

esp_err_t sys_event_bus_post(sys_event_id_t event_id, const void *event_data, size_t event_data_size)
{
    return esp_event_post(SYS_EVENT_BASE, event_id, event_data, event_data_size, pdMS_TO_TICKS(100));
}
