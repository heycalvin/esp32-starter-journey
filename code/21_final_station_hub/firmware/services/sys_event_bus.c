#include "sys_event_bus.h"
#include "esp_log.h"

static const char *TAG = "SYS_EVENT_BUS";
ESP_EVENT_DEFINE_BASE(SYS_HUB_EVENT_BASE);

esp_err_t sys_event_bus_init(void)
{
    esp_err_t ret = esp_event_loop_create_default();
    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "📢 [服务层] 全局系统统一事件总线 (Event Bus) 初始化成功！");
        return ESP_OK;
    }
    return ret;
}

esp_err_t sys_event_bus_post(hub_event_id_t id, const void *data, size_t size)
{
    return esp_event_post(SYS_HUB_EVENT_BASE, id, data, size, pdMS_TO_TICKS(100));
}
