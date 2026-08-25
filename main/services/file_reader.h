#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t file_reader_init(void);

// 1. 电子相册服务
int  file_reader_get_photo_count(void);
bool file_reader_get_photo_path(int index, char *out_path, size_t max_len);

// 2. 电子小说阅读器服务
esp_err_t file_reader_load_novel_page(int page_index, char *out_text_buf, size_t buf_size, int *out_total_pages);
int       file_reader_get_bookmark(void);
esp_err_t file_reader_set_bookmark(int page_index);

// 3. 传感器黑匣子日志追加
esp_err_t file_reader_append_sensor_log(const char *time_str, float temp, float humi, float dist);

#ifdef __cplusplus
}
#endif
