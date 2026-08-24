/**
 * ==============================================================================
 * 🚀 ESP32 物联网实战闯关 —— 第 17 关：OTA 空中固件升级与防变砖回滚
 * 📁 实验 1: Flash 自定义 A/B 双分区表检测与固件元数据诊断
 * ==============================================================================
 * 
 * 📌 【实验目标】
 * 1. 深入理解 Flash 分区表（Partition Table）布局与 A/B 乒乓备份机制；
 * 2. 读取当前正在运行的固件分区（Running Partition）与下一个目标升级分区（Next Update Partition）；
 * 3. 提取固件元数据（项目名、版本号、编译日期时间、ESP-IDF 版本及 SHA-256 唯一指纹）；
 * 4. 遍历并打印 Flash 中所有已定义的分区（NVS, OTA Data, ota_0, ota_1 等）。
 * 
 * 🔌 【硬件连接】
 * - 板载绿色 LED2: GPIO27 (系统就绪心跳指示灯)
 * ==============================================================================
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"

#define LED_PIN GPIO_NUM_27
static const char *TAG = "EXP1_OTA_CHECK";

/**
 * @brief 将分区类型转换为可读字符串
 */
static const char* partition_type_to_str(esp_partition_type_t type, esp_partition_subtype_t subtype) {
    if (type == ESP_PARTITION_TYPE_APP) {
        switch (subtype) {
            case ESP_PARTITION_SUBTYPE_APP_FACTORY: return "APP / Factory (出厂固件)";
            case ESP_PARTITION_SUBTYPE_APP_OTA_0:   return "APP / OTA_0 (A槽固件)";
            case ESP_PARTITION_SUBTYPE_APP_OTA_1:   return "APP / OTA_1 (B槽固件)";
            case ESP_PARTITION_SUBTYPE_APP_TEST:    return "APP / Test (测试固件)";
            default: return "APP / Unknown";
        }
    } else if (type == ESP_PARTITION_TYPE_DATA) {
        switch (subtype) {
            case ESP_PARTITION_SUBTYPE_DATA_OTA:      return "DATA / OTA Data (启动指向)";
            case ESP_PARTITION_SUBTYPE_DATA_NVS:      return "DATA / NVS (参数存储)";
            case ESP_PARTITION_SUBTYPE_DATA_PHY:      return "DATA / PHY (射频校准)";
            case ESP_PARTITION_SUBTYPE_DATA_FAT:      return "DATA / FAT (文件系统)";
            case ESP_PARTITION_SUBTYPE_DATA_SPIFFS:   return "DATA / SPIFFS";
            default: return "DATA / Custom";
        }
    }
    return "UNKNOWN";
}

/**
 * @brief 打印当前运行固件的详细元数据
 */
static void print_current_app_info(void) {
    const esp_app_desc_t *app_desc = esp_app_get_description();
    const esp_partition_t *running_part = esp_ota_get_running_partition();
    const esp_partition_t *next_part = esp_ota_get_next_update_partition(NULL);

    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "        📱 ESP32 当前运行固件元数据 (App Description)        ");
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "📦 项目名称 (Project Name) : %s", app_desc->project_name);
    ESP_LOGI(TAG, "🏷️ 固件版本 (App Version)  : %s", app_desc->version);
    ESP_LOGI(TAG, "⏰ 编译时间 (Compile Date) : %s %s", app_desc->date, app_desc->time);
    ESP_LOGI(TAG, "⚙️ IDF 版本 (ESP-IDF Ver) : %s", app_desc->idf_ver);

    // 格式化打印固件 SHA-256 指纹
    char sha256_str[65] = {0};
    for (int i = 0; i < 32; i++) {
        sprintf(&sha256_str[i * 2], "%02x", app_desc->app_elf_sha256[i]);
    }
    ESP_LOGI(TAG, "🔑 SHA-256 固件指纹        : %s", sha256_str);

    ESP_LOGI(TAG, "----------------------------------------------------------");
    if (running_part) {
        ESP_LOGI(TAG, "▶️ 当前运行分区 (Running)   : [%s] (起始地址: 0x%08lX, 大小: %lu KB)",
                 running_part->label, running_part->address, running_part->size / 1024);
    } else {
        ESP_LOGE(TAG, "❌ 无法获取当前运行分区！");
    }

    if (next_part) {
        ESP_LOGI(TAG, "🎯 下次升级目标 (Next Target): [%s] (起始地址: 0x%08lX, 大小: %lu KB)",
                 next_part->label, next_part->address, next_part->size / 1024);
    } else {
        ESP_LOGW(TAG, "⚠️ 未找到可升级的目标 OTA 分区！");
    }
    ESP_LOGI(TAG, "==========================================================\n");
}

/**
 * @brief 遍历并打印 Flash 中的所有分区表
 */
static void scan_and_print_all_partitions(void) {
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "          🗺️ ESP32 全局 Flash 分区表完整扫描清单             ");
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, " %-10s | %-8s | %-10s | %-10s | %-24s", "Label", "Type", "Offset", "Size (KB)", "Description");
    ESP_LOGI(TAG, "--------------------------------------------------------------------------------");

    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it != NULL) {
        const esp_partition_t *p = esp_partition_get(it);
        ESP_LOGI(TAG, " %-10s | 0x%02X     | 0x%08lX | %-10lu | %s",
                 p->label,
                 p->type,
                 p->address,
                 p->size / 1024,
                 partition_type_to_str(p->type, p->subtype));
        it = esp_partition_next(it);
    }
    esp_partition_iterator_release(it);
    ESP_LOGI(TAG, "==========================================================\n");
}

void app_main(void) {
    // 1. 初始化 NVS 闪存
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. 初始化板载 LED2 (GPIO27)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(LED_PIN, 0);

    ESP_LOGI(TAG, "🌟 Level 17 实验 1: OTA 分区检测与元数据诊断已启动！");

    // 3. 打印当前 App 详细信息与 Flash 分区表
    print_current_app_info();
    scan_and_print_all_partitions();

    // 4. 心跳主循环
    int loop_count = 0;
    while (1) {
        gpio_set_level(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(900));

        loop_count++;
        if (loop_count % 10 == 0) {
            const esp_partition_t *running = esp_ota_get_running_partition();
            ESP_LOGI(TAG, "💓 [心跳存活] 系统正常运行中... 当前分区: [%s], 运行时间: %d 秒",
                     running ? running->label : "Unknown", loop_count);
        }
    }
}
