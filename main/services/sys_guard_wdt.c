#include "sys_guard_wdt.h"
#include "esp_task_wdt.h"
#include "esp_log.h"

static const char *TAG = "SYS_GUARD";

esp_err_t sys_guard_wdt_init(uint32_t timeout_ms)
{
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = timeout_ms,
        .idle_core_mask = 0,
        .trigger_panic = true, // 超时触发内核 Panic 打印回溯调用栈
    };
    esp_err_t ret = esp_task_wdt_init(&twdt_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "🛡️ 硬件任务看门狗 (TWDT) 守护启动 (超时阀值: %lu ms)", timeout_ms);
    }
    return ret;
}

esp_err_t sys_guard_wdt_subscribe_current_task(void)
{
    return esp_task_wdt_add(NULL);
}

esp_err_t sys_guard_wdt_feed(void)
{
    return esp_task_wdt_reset();
}
