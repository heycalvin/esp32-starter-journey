#include "file_reader.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "bsp_sdcard.h"

static const char *TAG = "FILE_READER";
#define NOVEL_FILE_PATH "/sdcard/novel.txt"
#define LOG_FILE_PATH   "/sdcard/sensor.csv"
#define PAGE_BYTES_LEN  160 // 每页字符数

static int s_photo_count = 0;
static char s_photo_names[16][64];

esp_err_t file_reader_init(void)
{
    s_photo_count = 0;
    if (!bsp_sdcard_is_mounted()) {
        ESP_LOGW(TAG, "⚠️ TF 卡未就绪，相册与小说服务将使用内置默认演示数据");
        return ESP_OK;
    }

    // 扫描 /sdcard/photos 目录
    DIR *dir = opendir("/sdcard/photos");
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && s_photo_count < 16) {
            if (entry->d_type == DT_REG && strstr(entry->d_name, ".")) {
                snprintf(s_photo_names[s_photo_count], sizeof(s_photo_names[0]), "/sdcard/photos/%s", entry->d_name);
                s_photo_count++;
            }
        }
        closedir(dir);
        ESP_LOGI(TAG, "🖼️ [相册引擎] 扫描到 %d 张本地相片", s_photo_count);
    }

    // 初始化 sensor.csv 标题头
    FILE *f = fopen(LOG_FILE_PATH, "r");
    if (!f) {
        f = fopen(LOG_FILE_PATH, "w");
        if (f) {
            fprintf(f, "Timestamp,Temperature_C,Humidity_Pct,Distance_CM\n");
            fclose(f);
        }
    } else {
        fclose(f);
    }

    return ESP_OK;
}

int file_reader_get_photo_count(void)
{
    return (s_photo_count > 0) ? s_photo_count : 3; // 至少返回 3 张（内置/TF卡）
}

bool file_reader_get_photo_path(int index, char *out_path, size_t max_len)
{
    if (s_photo_count > 0 && index < s_photo_count) {
        strncpy(out_path, s_photo_names[index], max_len);
        return true;
    }
    snprintf(out_path, max_len, "Sample Photo #%d", index + 1);
    return false;
}

esp_err_t file_reader_load_novel_page(int page_index, char *out_text_buf, size_t buf_size, int *out_total_pages)
{
    if (!out_text_buf || buf_size == 0) return ESP_ERR_INVALID_ARG;

    if (!bsp_sdcard_is_mounted()) {
        const char *sample_novel = 
            "【示例小说：三体宇宙纪元】\n"
            "那是一个晴朗的午后，微风拂过控制中枢。\n"
            "太空望远镜在深空中捕捉到了一段规律的脉冲信号。\n"
            "文明的种子正在浩瀚的星河中悄然萌芽...\n"
            "(提示: 将 novel.txt 放入 TF卡根目录即可开启全本畅读)";
        strncpy(out_text_buf, sample_novel, buf_size);
        if (out_total_pages) *out_total_pages = 1;
        return ESP_OK;
    }

    FILE *f = fopen(NOVEL_FILE_PATH, "rb");
    if (!f) {
        snprintf(out_text_buf, buf_size, "未在 TF 卡中找到 novel.txt 文件\n请通过 Web 管理后台上传电子书！");
        if (out_total_pages) *out_total_pages = 1;
        return ESP_OK;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    int total_pages = (file_size + PAGE_BYTES_LEN - 1) / PAGE_BYTES_LEN;
    if (total_pages < 1) total_pages = 1;
    if (out_total_pages) *out_total_pages = total_pages;

    if (page_index >= total_pages) page_index = total_pages - 1;
    if (page_index < 0) page_index = 0;

    fseek(f, page_index * PAGE_BYTES_LEN, SEEK_SET);
    size_t read_bytes = fread(out_text_buf, 1, buf_size - 1, f);
    out_text_buf[read_bytes] = '\0';
    fclose(f);

    return ESP_OK;
}

esp_err_t file_reader_append_sensor_log(const char *time_str, float temp, float humi, float dist)
{
    if (!bsp_sdcard_is_mounted()) return ESP_ERR_INVALID_STATE;

    FILE *f = fopen(LOG_FILE_PATH, "a");
    if (!f) return ESP_FAIL;

    fprintf(f, "%s,%.2f,%.2f,%.2f\n", time_str ? time_str : "00:00:00", temp, humi, dist);
    fflush(f);
    fclose(f);
    return ESP_OK;
}
