#include "file_reader.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "bsp_sdcard.h"
#include "sys_font_manager.h"

static const char *TAG = "FILE_READER";
#define NOVEL_FILE_PATH "/sdcard/novel.txt"
#define LOG_FILE_PATH   "/sdcard/sensor.csv"
#define PAGE_BYTES_LEN  180 // 每页字符数

static int s_photo_count = 0;
static char s_photo_names[16][280];
static int s_saved_bookmark_page = 0;

static const char *s_demo_pages[] = {
    "【第一章 · 深空天线】\n"
    "这是一个宁静的午后，微风拂过星环中枢。\n"
    "巨大的射电望远镜缓缓转动，在宇宙深处捕捉到了一串极其诡异且规律的脉冲信号。\n"
    "人类文明的火种在浩瀚星河中悄然点亮...",

    "【第二章 · 黑暗森林】\n"
    "宇宙就是一座黑暗森林，每个文明都是带枪的猎人。\n"
    "他们像幽灵般潜行于林间，竭力不让脚步发出一点儿声音，因为林中到处都有和他一样的猎人。\n"
    "一旦发现目标，唯一的选择就是消灭它。",

    "【第三章 · 水滴降临】\n"
    "它只有三米多长，表面是全反射的绝对镜面。\n"
    "那是强相互作用力的奇迹，其原子被死死锁在一起，没有任何温度与振动。\n"
    "人类庞大的两千艘恒星级舰队，在这枚小小的水滴面前如同脆弱的纸片般灰飞烟灭。",

    "【第四章 · 掩体计划】\n"
    "既然恒星即将被毁灭，人类便躲藏到了巨行星的背面。\n"
    "在木星、土星、海王星的阴影里，数十座太空城拔地而起。\n"
    "人们在人造重力与人造阳光下生活，以为已经逃离了宇宙终极的杀机。",

    "【第五章 · 曲率驱动】\n"
    "光速并非不可跨越的墙壁，只要改变空间本身的曲率。\n"
    "飞船尾部抚平了空间的褶皱，被后方平坦空间以千分之一光秒的速度飞速弹射出去。\n"
    "航迹留下的死线，成为了宇宙中最深邃的伤痕。",

    "【第六章 · 二向箔打击】\n"
    "那是一张完全透明的小纸片，静静漂浮在虚空中。\n"
    "当力场消散的瞬间，三维空间开始向二维跌落。\n"
    "三维世界的物质被源源不断地压进二维平面，整个太阳系正在变成一幅壮丽绝伦的绝美画卷。",

    "【第七章 · 跌落二维】\n"
    "行星、彗星、阳光、飞船，全部失去了厚度。\n"
    "所有的细节在二维画卷上被纤毫毕现地展开，没有交叠，没有掩盖。\n"
    "我们看到了浩瀚星空最壮烈的落幕，宏伟而绝望。",

    "【第八章 · 宇宙归零】\n"
    "当大宇宙在降维中逐渐走向热寂，神秘的归零者发出了跨越所有星系的召唤广播。\n"
    "所有的超弦与小宇宙必须交出偷走的质量，让大宇宙在大挤压后重新诞生。\n"
    "文明交出了小宇宙，只留下了一个小小的生态球和五公斤的记忆体。",

    "【第九章 · 永远的星辰】\n"
    "给岁月以文明，而不是给文明以岁月。\n"
    "我看到了我的爱恋，我飞到了她的身边。\n"
    "在亿万光年的时空彼岸，星光依旧温柔地照耀着这颗孤独的蓝色星球。\n"
    "(全书完 · TF卡可存入百万字txt畅读)"
};

esp_err_t file_reader_init(void)
{
    s_photo_count = 0;

    // 读取 NVS 中的书签记录
    nvs_handle_t nvs_h;
    if (nvs_open("storage", NVS_READONLY, &nvs_h) == ESP_OK) {
        int32_t bm = 0;
        if (nvs_get_i32(nvs_h, "novel_bookmark", &bm) == ESP_OK) {
            s_saved_bookmark_page = bm;
        }
        nvs_close(nvs_h);
    }

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
    return (s_photo_count > 0) ? s_photo_count : 3;
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

int file_reader_get_bookmark(void)
{
    return s_saved_bookmark_page;
}

esp_err_t file_reader_set_bookmark(int page_index)
{
    s_saved_bookmark_page = page_index;
    nvs_handle_t nvs_h;
    if (nvs_open("storage", NVS_READWRITE, &nvs_h) == ESP_OK) {
        nvs_set_i32(nvs_h, "novel_bookmark", page_index);
        nvs_commit(nvs_h);
        nvs_close(nvs_h);
        ESP_LOGI(TAG, "🔖 书签已自动持久化至 NVS (页码: %d)", page_index + 1);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t file_reader_load_novel_page(int page_index, char *out_text_buf, size_t buf_size, int *out_total_pages)
{
    if (!out_text_buf || buf_size == 0) return ESP_ERR_INVALID_ARG;

    bool has_card = bsp_sdcard_is_mounted();
    FILE *f = has_card ? fopen(NOVEL_FILE_PATH, "rb") : NULL;
    long file_size = 0;
    if (f) {
        fseek(f, 0, SEEK_END);
        file_size = ftell(f);
        if (file_size < 30) {
            // TF 卡上的 novel.txt 文件为空或小于30字节，自动回退到内置9章《三体》
            fclose(f);
            f = NULL;
        }
    }

    if (!f) {
        int demo_count = sizeof(s_demo_pages) / sizeof(s_demo_pages[0]);
        if (out_total_pages) *out_total_pages = demo_count;
        if (page_index < 0) page_index = 0;
        if (page_index >= demo_count) page_index = demo_count - 1;

        strncpy(out_text_buf, s_demo_pages[page_index], buf_size - 1);
        out_text_buf[buf_size - 1] = '\0';
        ESP_LOGI(TAG, "📖 [内置长篇小说] 加载第 %d / %d 页: %s",
                 page_index + 1, demo_count, out_text_buf);
        return ESP_OK;
    }

    int total_pages = (file_size + PAGE_BYTES_LEN - 1) / PAGE_BYTES_LEN;
    if (total_pages < 1) total_pages = 1;
    if (out_total_pages) *out_total_pages = total_pages;

    if (page_index >= total_pages) page_index = total_pages - 1;
    if (page_index < 0) page_index = 0;

    fseek(f, page_index * PAGE_BYTES_LEN, SEEK_SET);
    size_t to_read = (buf_size - 1 < PAGE_BYTES_LEN) ? (buf_size - 1) : PAGE_BYTES_LEN;
    size_t read_bytes = fread(out_text_buf, 1, to_read, f);
    fclose(f);

    // UTF-8 安全防截断处理
    while (read_bytes > 0 && (out_text_buf[read_bytes - 1] & 0xC0) == 0x80) {
        read_bytes--;
    }
    if (read_bytes > 0 && (out_text_buf[read_bytes - 1] & 0xC0) == 0xC0) {
        read_bytes--;
    }
    out_text_buf[read_bytes] = '\0';

    ESP_LOGI(TAG, "📖 [TF卡小说] 加载第 %d / %d 页 (读取 %d 字节)", page_index + 1, total_pages, (int)read_bytes);
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
