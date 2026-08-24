/**
 * ==============================================================================
 * 🚀 ESP32 物联网实战闯关 —— 第 18 关：MicroSD/TF 卡挂载与 FATFS 文件系统
 * 📁 实验 2: 工业级传感器黑匣子 CSV 数据记录仪 (Sensor Data Logger)
 * ==============================================================================
 * 
 * 📌 【实验目标】
 * 1. 掌握工业级数据持久化核心落盘规范（CSV 逗号分隔格式）；
 * 2. 掌握 `fopen(..., "a")` 追加写入模式与文件存在性检测；
 * 3. 掌握 `fflush()` 缓存及时刷盘技巧，彻底规避突然断电丢数据的重大风险；
 * 4. 模拟周期性将开机时间戳、模拟温度、超声波距离与系统内存余量按时序录入 TF 卡。
 * ==============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_timer.h"
#include "esp_log.h"

#define MOUNT_POINT "/sdcard"
#define CSV_FILE_PATH MOUNT_POINT "/sensor_log.csv"
static const char *TAG = "EXP2_DATALOGGER";

/**
 * @brief 挂载 TF 卡辅助函数
 */
static bool init_sd_card(sdmmc_card_t **out_card)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, out_card);
    if (ret != ESP_OK) {
        slot_config.width = 1;
        ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, out_card);
    }
    return (ret == ESP_OK);

}

void app_main(void)
{
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "   📊 Level 18 实验 2：工业级传感器 CSV 黑匣子记录仪      ");
    ESP_LOGI(TAG, "==========================================================");

    sdmmc_card_t *card = NULL;
    if (!init_sd_card(&card)) {
        ESP_LOGE(TAG, "❌ TF 卡挂载失败！请插入格式化为 FAT32 的 MicroSD 卡后重试！");
        return;
    }
    ESP_LOGI(TAG, "✅ TF 卡已成功接入！黑匣子日志文件: [%s]", CSV_FILE_PATH);

    // 1. 检查目标 CSV 文件是否已存在
    struct stat st;
    bool file_exists = (stat(CSV_FILE_PATH, &st) == 0);

    // 2. 以追加模式 ("a") 打开文件，指针自动定位到文件末尾
    FILE *f = fopen(CSV_FILE_PATH, "a");
    if (!f) {
        ESP_LOGE(TAG, "❌ 无法打开 CSV 文件进行写入！");
        return;
    }

    // 3. 如果是新创建的文件，先写入标准的 CSV 表头 (Header)
    if (!file_exists || st.st_size == 0) {
        fprintf(f, "Timestamp_Sec,Temperature_C,Distance_CM,Free_Heap_Bytes,Status\n");
        fflush(f); // 强制刷盘写入物理闪存
        ESP_LOGI(TAG, "📝 检测到新文件，已成功写入 CSV 英文表头！");
    } else {
        ESP_LOGI(TAG, "📝 检测到已有日志（当前大小: %ld 字节），将在末尾继续追加记录...", st.st_size);
    }

    // 4. 周期性模拟写入 10 条高精度传感器时序记录
    ESP_LOGI(TAG, "🚀 开始周期性写入传感器监测数据 (每秒 1 条，共 10 条)...");
    for (int i = 1; i <= 10; i++) {
        int64_t uptime_sec = esp_timer_get_time() / 1000000;
        float mock_temp = 25.0f + (float)(i % 5) * 0.35f;
        float mock_distance = 45.2f + (float)(i % 3) * 1.8f;
        uint32_t free_heap = esp_get_free_heap_size();

        // 写入标准逗号分隔行：时间戳,温度,距离,空闲内存,状态
        fprintf(f, "%lld,%.2f,%.2f,%lu,OK\n",
                uptime_sec, mock_temp, mock_distance, (unsigned long)free_heap);
        
        // ⚠️ 核心内功：fflush() 强制将 C 库内部的 RAM 缓冲区推入 SD 卡物理闪存！
        fflush(f);

        ESP_LOGI(TAG, "📥 [写入第 %02d/10 条] 时间: %4llds | 温度: %5.2f°C | 测距: %5.2fcm | 剩余RAM: %lu B",
                 i, uptime_sec, mock_temp, mock_distance, (unsigned long)free_heap);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // 5. 安全关闭文件句柄
    fclose(f);
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "🎉 10 条传感器记录已全部安全落盘！你可以随时拔出 TF 卡插到电脑上用 Excel 打开查看！");
    ESP_LOGI(TAG, "==========================================================");
}
