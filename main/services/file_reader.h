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
void file_reader_rescan_photos(void);
bool file_reader_decode_jpeg_to_buffer(const char *filepath, uint16_t *dst_buf, int dst_w, int dst_h);

// 2. 电子小说阅读器服务
typedef struct {
    int page_index;          // 对应页码 (0-indexed)
    long file_offset;        // 文件字节偏移量
    char title[48];          // 章节标题名称
} novel_chapter_t;

esp_err_t file_reader_load_novel_page(int page_index, char *out_text_buf, size_t buf_size, int *out_total_pages);
int       file_reader_get_bookmark(void);
esp_err_t file_reader_set_bookmark(int page_index);
int       file_reader_get_chapter_count(void);
const novel_chapter_t *file_reader_get_chapter(int index);
void      file_reader_rescan_chapters(void);

// 3. 传感器黑匣子日志追加
esp_err_t file_reader_append_sensor_log(const char *time_str, float temp, float humi, float dist);

#ifdef __cplusplus
}
#endif
