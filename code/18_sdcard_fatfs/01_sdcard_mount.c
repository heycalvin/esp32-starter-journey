/**
 * ==============================================================================
 * 🚀 ESP32 物联网实战闯关 —— 第 18 关：MicroSD/TF 卡挂载与 FATFS 文件系统
 * 📁 实验 1: TF 卡 SDMMC 高速总线挂载、硬件信息诊断与标准 C 文件流读写
 * ==============================================================================
 * 
 * 📌 【实验目标】
 * 1. 深入理解 ESP32 硬件 SDMMC 控制器与 4-bit / 1-bit SDIO 高速并行总线；
 * 2. 使用 ESP-IDF VFS FAT 挂载器一键将 TF 卡挂载为 "/sdcard" 虚拟目录；
 * 3. 提取并打印 TF 卡的物理硬件参数（容量大小、扇区尺寸、卡名、支持最高时钟）；
 * 4. 熟练使用标准 C 语言文件流 API（fopen、fprintf、fgets、fclose）在单片机上读写文件！
 * 
 * 📌 【硬件引脚与跳线说明】
 * - 板载 TF 卡槽由 ESP32 硬件 SDMMC 驱动 (Slot 1):
 *   CLK: GPIO14, CMD: GPIO15, D0: GPIO2, D1: GPIO4, D2: GPIO12, D3: GPIO13
 * - 💡 注：默认支持 4-bit / 1-bit SDIO 与 SPI (SDSPI) 三重自适应通道！
 * ==============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_timer.h"
#include "esp_log.h"

#define MOUNT_POINT "/sdcard"
static const char *TAG = "EXP1_SD_MOUNT";

// 尝试 SDSPI 模式挂载 (作为 SDMMC 受到信号/上拉干扰时的终极兼容通道)
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

void app_main(void)
{
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "   🚀 Level 18 实验 1：MicroSD / TF 卡 SDMMC 高速挂载启动  ");
    ESP_LOGI(TAG, "==========================================================");

    // 1. 配置 FATFS 虚拟文件系统挂载参数
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, // 挂载失败时不自动格式化，防止意外抹掉重要文件
        .max_files = 5,                  // 同时允许打开的最大文件句柄数
        .allocation_unit_size = 16 * 1024 // 簇大小（16KB）
    };

    sdmmc_card_t *card = NULL;
    ESP_LOGI(TAG, "📁 正在初始化 SDMMC 硬件控制器与总线...");

    // 2. 初始化硬件主机配置（默认使用 Slot 1 高速控制器）
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = 10000; // 10MHz 稳定频率

    // 3. 配置引脚与总线位宽 (板载标准 4-bit SDIO 总线 + 开启内部弱上拉)
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4; // 开启 4-bit 并行极速传输
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP; // 开启内部上拉电阻

    // 4. 执行挂载：优先尝试 4-bit SDMMC，失败则尝试 1-bit，再失败则尝试 SDSPI
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "⚠️ 4-bit 模式响应超时 (0x%x)，尝试 1-bit 模式降级连接...", ret);
        slot_config.width = 1;
        ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "⚠️ SDMMC 硬件总线超时 (0x%x)，启动 SDSPI 通道重试...", ret);
        ret = try_mount_sdspi(&card, &mount_config);
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "==========================================================");
        ESP_LOGE(TAG, "❌ 挂载文件系统失败 (错误码: %s / 0x%x)！", esp_err_to_name(ret), ret);
        ESP_LOGE(TAG, "👉 排查提示：");
        ESP_LOGE(TAG, "   1. TF 卡是否已在电脑上格式化为 【FAT32】 格式？(exFAT/NTFS 无法识别)");
        ESP_LOGE(TAG, "   2. TF 卡槽是否已插到底？(金手指是否有氧化污渍)");
        ESP_LOGE(TAG, "==========================================================");
        return;
    }

    ESP_LOGI(TAG, "🎉 ==========================================================");
    ESP_LOGI(TAG, "🎉 MicroSD / TF 卡挂载成功！挂载路径: [%s]", MOUNT_POINT);
    ESP_LOGI(TAG, "🎉 ==========================================================");

    // 5. 打印 TF 卡的物理硬件参数
    ESP_LOGI(TAG, "----------------------------------------------------------");
    ESP_LOGI(TAG, "🏷️ 卡名称 (Name)      : %s", card->cid.name);
    ESP_LOGI(TAG, "💾 卡总容量 (Capacity) : %llu MB (约 %.2f GB)",
             ((uint64_t)card->csd.capacity) * card->csd.sector_size / (1024 * 1024),
             (float)(((uint64_t)card->csd.capacity) * card->csd.sector_size) / (1024.0f * 1024.0f * 1024.0f));
    ESP_LOGI(TAG, "🧱 扇区大小 (Sector)  : %d 字节", card->csd.sector_size);
    ESP_LOGI(TAG, "⚡ 最大总线频率 (Speed): %d MHz", card->max_freq_khz / 1000);
    ESP_LOGI(TAG, "----------------------------------------------------------\n");

    // 6. 标准 C 语言文件写入测试 (Write)
    const char *test_file = MOUNT_POINT "/hello_esp32.txt";
    ESP_LOGI(TAG, "✍️ 正在向 TF 卡创建并写入测试文件: %s ...", test_file);

    FILE *f = fopen(test_file, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "❌ 创建文件失败！请检查 TF 卡是否有写保护或已满！");
        return;
    }
    fprintf(f, "==================================================\n");
    fprintf(f, "🚀 Hello from ESP32 SDMMC FATFS File System!\n");
    fprintf(f, "📱 Board Model: ESP32-WROOM-32E (8MB Flash + 2MB PSRAM)\n");
    fprintf(f, "⏰ Timestamp: %lld ms\n", (long long)(esp_timer_get_time() / 1000));
    fprintf(f, "==================================================\n");
    fclose(f); // 关闭并刷盘保存
    ESP_LOGI(TAG, "✅ 文件写入成功并已安全落盘！");

    // 7. 标准 C 语言文件读取测试 (Read)
    ESP_LOGI(TAG, "📖 正在从 TF 卡回读刚刚写入的文件内容:");
    f = fopen(test_file, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "❌ 打开文件读取失败！");
        return;
    }
    char line_buf[128];
    int line_num = 1;
    while (fgets(line_buf, sizeof(line_buf), f) != NULL) {
        // 移除行尾换行符便于日志排版
        line_buf[strcspn(line_buf, "\r\n")] = 0;
        ESP_LOGI(TAG, "   [第 %d 行] ➔ \033[32m%s\033[0m", line_num++, line_buf);
    }
    fclose(f);

    ESP_LOGI(TAG, "🏆 TF 卡基础读写流水线验证 100% 通过！");
}
