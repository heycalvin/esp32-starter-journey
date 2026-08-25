#include "sys_ota.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_log.h"

static const char *TAG = "SYS_OTA";
static esp_ota_handle_t s_ota_handle = 0;
static const esp_partition_t *s_update_partition = NULL;

esp_err_t sys_ota_init(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "🔍 [OTA 诊断] 新固件启动自检通过，确认有效并取消自动回退！");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }
    ESP_LOGI(TAG, "🚀 [服务层] OTA 双模升级引擎就绪，当前运行分区: [%s]", running ? running->label : "factory");
    return ESP_OK;
}

esp_err_t sys_ota_begin_upgrade(size_t image_size)
{
    s_update_partition = esp_ota_get_next_update_partition(NULL);
    if (!s_update_partition) {
        ESP_LOGE(TAG, "❌ 未找到可写入的目标 OTA 备用分区！");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "📦 正在擦除目标 OTA 分区: [%s] (大小: %d 字节)...", s_update_partition->label, (int)image_size);
    esp_err_t ret = esp_ota_begin(s_update_partition, image_size, &s_ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ esp_ota_begin 失败 (0x%x)", ret);
    }
    return ret;
}

esp_err_t sys_ota_write_chunk(const void *data, size_t size)
{
    if (s_ota_handle == 0) return ESP_ERR_INVALID_STATE;
    return esp_ota_write(s_ota_handle, data, size);
}

esp_err_t sys_ota_finish_and_reboot(void)
{
    if (s_ota_handle == 0 || !s_update_partition) return ESP_ERR_INVALID_STATE;

    esp_err_t ret = esp_ota_end(s_ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ esp_ota_end 校验失败 (0x%x)", ret);
        return ret;
    }

    ret = esp_ota_set_boot_partition(s_update_partition);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ esp_ota_set_boot_partition 失败 (0x%x)", ret);
        return ret;
    }

    ESP_LOGI(TAG, "🎉 [OTA 成功] 固件写入与校验完成，设置下一启动分区为 [%s]，2秒后重启！", s_update_partition->label);
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}

esp_err_t sys_ota_save_resource_file(const char *filepath, const void *data, size_t size)
{
    if (!filepath || !data || size == 0) return ESP_ERR_INVALID_ARG;
    FILE *f = fopen(filepath, "wb");
    if (!f) {
        ESP_LOGE(TAG, "❌ 无法打开资源文件写入: %s", filepath);
        return ESP_FAIL;
    }
    fwrite(data, 1, size, f);
    fclose(f);
    ESP_LOGI(TAG, "💾 [资源 OTA] 成功写入资源包: %s (大小: %d 字节)", filepath, (int)size);
    return ESP_OK;
}
