#include "bsp_sdcard.h"
#include <stdio.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_log.h"

static const char *TAG = "BSP_SDCARD";
#define MOUNT_POINT "/sdcard"

static bool s_is_mounted = false;
static sdmmc_card_t *s_card = NULL;

static esp_err_t try_mount_sdspi(sdmmc_card_t **out_card, const esp_vfs_fat_sdmmc_mount_config_t *mount_config)
{
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

esp_err_t bsp_sdcard_init(void)
{
    if (s_is_mounted) return ESP_OK;

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

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK) {
        slot_config.width = 1;
        ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);
    }
    if (ret != ESP_OK) {
        ret = try_mount_sdspi(&s_card, &mount_config);
    }

    if (ret == ESP_OK) {
        s_is_mounted = true;
        ESP_LOGI(TAG, "💾 [BSP] MicroSD/TF 卡挂载成功! (容量: %llu MB)", ((uint64_t)s_card->csd.capacity) * s_card->csd.sector_size / (1024 * 1024));
        mkdir("/sdcard/photos", 0777);
        mkdir("/sdcard/fonts", 0777);
    } else {
        ESP_LOGW(TAG, "⚠️ [BSP] 未检测到 TF 卡或挂载失败 (可不插入继续运行)");
    }
    return ret;
}

bool bsp_sdcard_is_mounted(void)
{
    return s_is_mounted;
}

esp_err_t bsp_sdcard_get_space_mb(uint32_t *total_mb, uint32_t *free_mb)
{
    if (!s_is_mounted || !s_card) return ESP_ERR_INVALID_STATE;

    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    FRESULT res = f_getfree("0:", &fre_clust, &fs);
    if (res == FR_OK) {
        tot_sect = (fs->n_fatent - 2) * fs->csize;
        fre_sect = fre_clust * fs->csize;
        if (total_mb) *total_mb = tot_sect / (2048);
        if (free_mb) *free_mb = fre_sect / (2048);
        return ESP_OK;
    }
    return ESP_FAIL;
}
