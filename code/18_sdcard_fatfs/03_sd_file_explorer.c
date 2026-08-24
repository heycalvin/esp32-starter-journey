/**
 * ==============================================================================
 * 🚀 ESP32 物联网实战闯关 —— 第 18 关：MicroSD/TF 卡挂载与 FATFS 文件系统
 * 📁 实验 3: TF 卡文件资源树递归扫描器与电子相册资源探针 (File Tree Explorer)
 * ==============================================================================
 * 
 * 📌 【实验目标】
 * 1. 熟练掌握 POSIX 标准目录遍历 API（opendir、readdir、closedir、stat）；
 * 2. 递归遍历整张 TF 卡根目录下所有子文件夹与多级文件，绘制出漂亮的树状层级图；
 * 3. 过滤 Mac/Windows 隐藏系统垃圾文件（如 .Spotlight、.Trashes），防止栈溢出；
 * 4. 采用独立 8KB 大栈深 FreeRTOS 任务运行深度扫描，彻底规避 Stack Overflow；
 * 5. 统计并计算每个文件的大小（B、KB、MB）、全盘总文件数与总占用空间！
 * ==============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"

#define MOUNT_POINT "/sdcard"
static const char *TAG = "EXP3_SD_EXPLORER";

static int s_total_files = 0;
static int s_total_dirs = 0;
static uint64_t s_total_bytes = 0;

// 尝试 SDSPI 模式挂载 (作为 SDMMC 兼容通道)
static esp_err_t try_mount_sdspi(sdmmc_card_t **out_card, const esp_vfs_fat_sdmmc_mount_config_t *mount_config) {
    ESP_LOGI(TAG, "🔄 正在尝试通过 SPI 兼容模式 (SDSPI) 挂载 TF 卡 (CLK:14, MOSI:15, MISO:2, CS:13)...");
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.max_freq_khz = 10000;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = GPIO_NUM_15,
        .miso_io_num = GPIO_NUM_2,
        .sclk_io_num = GPIO_NUM_14,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = GPIO_NUM_13;
    slot_config.host_id = host.slot;

    return esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, mount_config, out_card);
}

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
    host.max_freq_khz = 10000;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, out_card);
    if (ret != ESP_OK) {
        slot_config.width = 1;
        ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, out_card);
    }
    if (ret != ESP_OK) {
        ret = try_mount_sdspi(out_card, &mount_config);
    }
    return (ret == ESP_OK);
}

/**
 * @brief 递归扫描目录树并格式化输出（带深度限制与隐藏系统文件过滤）
 * @param dir_path 当前遍历目录路径
 * @param depth 递归深度
 */
static void scan_directory_tree(const char *dir_path, int depth)
{
    // 防御性保护：限制最大递归深度为 6 层，防止超深目录吃光栈内存
    if (depth > 6) {
        return;
    }

    DIR *d = opendir(dir_path);
    if (!d) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        // 1. 跳过 "." (当前目录) 与 ".." (上级目录)
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 2. 智能过滤 Mac/Windows 系统的隐藏深层索引文件夹（如 .Spotlight-V100、.fseventsd、.Trashes）
        if (entry->d_name[0] == '.') {
            continue;
        }

        // 动态构建完整路径
        char full_path[384];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0) {
            // 生成树状缩进前缀
            char indent[48] = {0};
            for (int i = 0; i < depth; i++) {
                strcat(indent, "  │ ");
            }

            if (S_ISDIR(st.st_mode)) {
                s_total_dirs++;
                ESP_LOGI(TAG, "%s├── 📁 \033[1;33m[%s]\033[0m (文件夹)", indent, entry->d_name);
                // 递归深入遍历下一级子目录
                scan_directory_tree(full_path, depth + 1);
            } else {
                s_total_files++;
                s_total_bytes += st.st_size;

                if (st.st_size >= 1024 * 1024) {
                    float size_mb = (float)st.st_size / (1024.0f * 1024.0f);
                    ESP_LOGI(TAG, "%s├── 📄 \033[36m%-24s\033[0m \033[90m(%.2f MB)\033[0m",
                             indent, entry->d_name, size_mb);
                } else if (st.st_size >= 1024) {
                    float size_kb = (float)st.st_size / 1024.0f;
                    ESP_LOGI(TAG, "%s├── 📄 \033[36m%-24s\033[0m \033[90m(%.2f KB)\033[0m",
                             indent, entry->d_name, size_kb);
                } else {
                    ESP_LOGI(TAG, "%s├── 📄 \033[36m%-24s\033[0m \033[90m(%ld Bytes)\033[0m",
                             indent, entry->d_name, st.st_size);
                }
            }
        }
    }

    closedir(d);
}

/**
 * @brief 独立高栈深扫描任务（8KB 栈空间，彻底免疫栈溢出）
 */
static void explorer_task(void *pvParam)
{
    s_total_files = 0;
    s_total_dirs = 0;
    s_total_bytes = 0;

    ESP_LOGI(TAG, "🔍 正在对 TF 卡进行深度递归扫描...\n");
    ESP_LOGI(TAG, "📂 [TF 卡根目录: %s]", MOUNT_POINT);

    scan_directory_tree(MOUNT_POINT, 0);

    ESP_LOGI(TAG, "\n==========================================================");
    ESP_LOGI(TAG, "📊 【TF 卡全盘资源扫描统计清单】");
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "📁 文件夹总数 (Directories) : %d 个", s_total_dirs);
    ESP_LOGI(TAG, "📄 文件总数 (Files)         : %d 个", s_total_files);
    ESP_LOGI(TAG, "💾 扫描文件总大小 (Total)    : %.2f KB (约 %.2f MB)",
             (float)s_total_bytes / 1024.0f,
             (float)s_total_bytes / (1024.0f * 1024.0f));
    ESP_LOGI(TAG, "==========================================================");

    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "   🌲 Level 18 实验 3：TF 卡全盘目录树与资源浏览器        ");
    ESP_LOGI(TAG, "==========================================================");

    sdmmc_card_t *card = NULL;
    if (!init_sd_card(&card)) {
        ESP_LOGE(TAG, "❌ TF 卡挂载失败！请插入 MicroSD 卡后重试！");
        return;
    }

    // 启动独立高栈深扫描任务 (8192 字节)，确保多层递归安全
    xTaskCreate(&explorer_task, "explorer_task", 8192, NULL, 5, NULL);
}
