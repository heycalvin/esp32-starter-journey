#include "sys_event_bus.h"
#include "esp_log.h"

static const char *TAG = "SYS_EVENT_BUS";

// 真正落地分配事件基底全局唯一内存指针
ESP_EVENT_DEFINE_BASE(SYS_EVENT_BASE);

esp_err_t sys_event_bus_init(void)
{
    esp_err_t ret = esp_event_loop_create_default();
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "全局事件总线已经存在，无需重复创建");
        return ESP_OK;
    }
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "📢 [服务层] 全局统一事件总线 (Event Bus) 初始化成功！");
    }
    return ret;
}

esp_err_t sys_event_bus_post(sys_event_id_t event_id, const void *event_data, size_t event_data_size)
{
    return esp_event_post(SYS_EVENT_BASE, event_id, event_data, event_data_size, portMAX_DELAY);
}
