#include "srv_event_bus.h"
#include "esp_log.h"

static const char *TAG = "SRV_EVENT";

ESP_EVENT_DEFINE_BASE(HUB_EVENT_BASE);

esp_err_t srv_event_bus_init(void)
{
    esp_err_t ret = esp_event_loop_create_default();
    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "✅ 超级中控全局事件总线就绪");
        return ESP_OK;
    }
    return ret;
}

esp_err_t srv_event_bus_post(hub_event_id_t id, const void *data, size_t size)
{
    return esp_event_post(HUB_EVENT_BASE, id, data, size, pdMS_TO_TICKS(100));
}
