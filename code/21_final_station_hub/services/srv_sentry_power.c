#include "srv_sentry_power.h"
#include "esp_task_wdt.h"
#include "esp_log.h"

static const char *TAG = "SRV_SENTRY";

esp_err_t srv_sentry_power_init(void)
{
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 8000,
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    esp_err_t ret = esp_task_wdt_init(&twdt_config);
    if (ret == ESP_ERR_INVALID_STATE) {
        esp_task_wdt_reconfigure(&twdt_config);
    }
    esp_task_wdt_add(NULL);
    ESP_LOGI(TAG, "🛡️ 电源与看门狗哨兵已就绪 (8秒看门狗)");
    return ESP_OK;
}

void srv_sentry_feed_dog(void)
{
    esp_task_wdt_reset();
}
