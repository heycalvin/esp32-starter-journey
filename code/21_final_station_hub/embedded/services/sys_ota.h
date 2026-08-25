#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

esp_err_t sys_ota_init(void);
esp_err_t sys_ota_begin_upgrade(size_t image_size);
esp_err_t sys_ota_write_chunk(const void *data, size_t size);
esp_err_t sys_ota_finish_and_reboot(void);
esp_err_t sys_ota_save_resource_file(const char *filepath, const void *data, size_t size);
