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
#include "esp32/rom/tjpgd.h"

static const char *TAG = "FILE_READER";
#define NOVEL_FILE_PATH "/sdcard/novel.txt"
#define LOG_FILE_PATH   "/sdcard/sensor.csv"
#define PAGE_BYTES_LEN  480 // 每页字符数 (扩大容量让排版饱满自如)

static void sanitize_novel_text(char *str)
{
    if (!str) return;
    char *src = str;
    char *dst = str;
    int consecutive_nl = 0;

    // 1. 跳过开头的空白字符
    while (*src == ' ' || *src == '\t' || *src == '\r' || *src == '\n') {
        src++;
    }

    // 2. 逐字符清洗 (消除 Windows \r 干扰，压缩多余空行)
    while (*src) {
        if (*src == '\r') {
            src++;
            continue;
        }
        if (*src == '\n') {
            consecutive_nl++;
            if (consecutive_nl <= 1) { // 严格限制至多单个换行
                *dst++ = '\n';
            }
            src++;
            while (*src == ' ' || *src == '\t') src++;
            continue;
        }

        consecutive_nl = 0;
        *dst++ = *src++;
    }

    // 3. 去除末尾的换行和空格
    while (dst > str && (*(dst - 1) == ' ' || *(dst - 1) == '\t' || *(dst - 1) == '\r' || *(dst - 1) == '\n')) {
        dst--;
    }
    *dst = '\0';
}

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

#define MAX_CHAPTERS 64
static novel_chapter_t s_chapters[MAX_CHAPTERS];
static int s_chapter_count = 0;

void file_reader_rescan_chapters(void)
{
    s_chapter_count = 0;
    if (!bsp_sdcard_is_mounted()) {
        s_chapters[0] = (novel_chapter_t){ .page_index = 0, .file_offset = 0, .title = "第一章 · 深空天线" };
        s_chapters[1] = (novel_chapter_t){ .page_index = 1, .file_offset = 180, .title = "第二章 · 黑暗森林" };
        s_chapters[2] = (novel_chapter_t){ .page_index = 2, .file_offset = 360, .title = "第三章 · 水滴降临" };
        s_chapters[3] = (novel_chapter_t){ .page_index = 3, .file_offset = 540, .title = "第四章 · 掩体计划" };
        s_chapters[4] = (novel_chapter_t){ .page_index = 4, .file_offset = 720, .title = "第五章 · 曲率驱动" };
        s_chapters[5] = (novel_chapter_t){ .page_index = 5, .file_offset = 900, .title = "第六章 · 二向箔打击" };
        s_chapters[6] = (novel_chapter_t){ .page_index = 6, .file_offset = 1080, .title = "第七章 · 跌落二维" };
        s_chapters[7] = (novel_chapter_t){ .page_index = 7, .file_offset = 1260, .title = "第八章 · 宇宙归零" };
        s_chapters[8] = (novel_chapter_t){ .page_index = 8, .file_offset = 1440, .title = "第九章 · 永远星辰" };
        s_chapter_count = 9;
        return;
    }

    FILE *f = fopen(NOVEL_FILE_PATH, "r");
    if (!f) return;

    char line[128];
    long offset = 0;
    while (fgets(line, sizeof(line), f) != NULL && s_chapter_count < MAX_CHAPTERS) {
        bool is_chapter = false;
        if (strstr(line, "【第") || strstr(line, "Chapter") || strstr(line, "引子") ||
            strstr(line, "序章") || strstr(line, "尾声") ||
            (strstr(line, "第") && (strstr(line, "章") || strstr(line, "卷") || strstr(line, "回")))) {
            is_chapter = true;
        }

        if (is_chapter) {
            char *p_nl = strpbrk(line, "\r\n");
            if (p_nl) *p_nl = '\0';
            
            char *p_title = line;
            while (*p_title == ' ' || *p_title == '\t' || *p_title == '[' || *p_title == '(') p_title++;

            // 跳过可能存在的中文【
            if ((unsigned char)p_title[0] == 0xE3 && (unsigned char)p_title[1] == 0x80 && (unsigned char)p_title[2] == 0x90) {
                p_title += 3;
            }

            // 截断尾部的 】
            char *p_bracket = strstr(p_title, "】");
            if (p_bracket) *p_bracket = '\0';
            char *p_end_br = strpbrk(p_title, "])");
            if (p_end_br) *p_end_br = '\0';

            if (strlen(p_title) > 0) {
                s_chapters[s_chapter_count].page_index = (int)(offset / PAGE_BYTES_LEN);
                s_chapters[s_chapter_count].file_offset = offset;
                
                // 最多保留 36 字节
                strncpy(s_chapters[s_chapter_count].title, p_title, sizeof(s_chapters[s_chapter_count].title) - 1);
                s_chapters[s_chapter_count].title[sizeof(s_chapters[s_chapter_count].title) - 1] = '\0';

                // 安全截断 UTF-8 尾部
                size_t tlen = strlen(s_chapters[s_chapter_count].title);
                while (tlen > 0 && (s_chapters[s_chapter_count].title[tlen - 1] & 0xC0) == 0x80) tlen--;
                if (tlen > 0 && (s_chapters[s_chapter_count].title[tlen - 1] & 0xC0) == 0xC0) tlen--;
                s_chapters[s_chapter_count].title[tlen] = '\0';

                s_chapter_count++;
            }
        }
        offset = ftell(f);
    }
    fclose(f);
    ESP_LOGI(TAG, "📑 [智能目录引擎] 成功从 novel.txt 索引了 %d 个精简章节！", s_chapter_count);
}

int file_reader_get_chapter_count(void)
{
    return s_chapter_count;
}

const novel_chapter_t *file_reader_get_chapter(int index)
{
    if (index >= 0 && index < s_chapter_count) {
        return &s_chapters[index];
    }
    return NULL;
}

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

    file_reader_rescan_chapters();

    if (!bsp_sdcard_is_mounted()) {
        ESP_LOGW(TAG, "⚠️ TF 卡未就绪，相册与小说服务将使用内置默认演示数据");
        return ESP_OK;
    }

    file_reader_rescan_photos();
    return ESP_OK;
}

void file_reader_rescan_photos(void)
{
    s_photo_count = 0;
    if (!bsp_sdcard_is_mounted()) return;

    DIR *dir = opendir("/sdcard/photos");
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && s_photo_count < 16) {
            if (entry->d_type == DT_REG && (strstr(entry->d_name, ".jpg") || strstr(entry->d_name, ".jpeg") || strstr(entry->d_name, ".png"))) {
                snprintf(s_photo_names[s_photo_count], sizeof(s_photo_names[0]), "/sdcard/photos/%s", entry->d_name);
                s_photo_count++;
            }
        }
        closedir(dir);
        ESP_LOGI(TAG, "🖼️ [相册引擎] 动态重新索引了 %d 张本地相片", s_photo_count);
    }
}

typedef struct {
    FILE *fp;
    uint16_t *out_buf;
    int target_w;
    int target_h;
} jpeg_io_device_t;

static UINT jpeg_input_func(JDEC *jd, BYTE *buff, UINT ndata)
{
    jpeg_io_device_t *dev = (jpeg_io_device_t *)jd->device;
    if (!dev || !dev->fp) return 0;
    if (buff) {
        return (UINT)fread(buff, 1, ndata, dev->fp);
    } else {
        return (fseek(dev->fp, ndata, SEEK_CUR) == 0) ? ndata : 0;
    }
}

static UINT jpeg_output_func(JDEC *jd, void *bitmap, JRECT *rect)
{
    jpeg_io_device_t *dev = (jpeg_io_device_t *)jd->device;
    uint8_t *src_rgb = (uint8_t *)bitmap;
    uint16_t *dst_buf = dev->out_buf;
    int dst_w = dev->target_w;
    int dst_h = dev->target_h;

    int rect_w = rect->right - rect->left + 1;
    int rect_h = rect->bottom - rect->top + 1;

    for (int y = 0; y < rect_h; y++) {
        int dst_y = rect->top + y;
        if (dst_y >= dst_h) break;

        for (int x = 0; x < rect_w; x++) {
            int dst_x = rect->left + x;
            if (dst_x >= dst_w) break;

            uint8_t r = src_rgb[(y * rect_w + x) * 3 + 0];
            uint8_t g = src_rgb[(y * rect_w + x) * 3 + 1];
            uint8_t b = src_rgb[(y * rect_w + x) * 3 + 2];

            dst_buf[dst_y * dst_w + dst_x] = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
        }
    }
    return 1;
}

bool file_reader_decode_jpeg_to_buffer(const char *filepath, uint16_t *dst_buf, int dst_w, int dst_h)
{
    if (!filepath || !dst_buf) return false;

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "❌ 无法打开图片文件: %s", filepath);
        return false;
    }

    const size_t pool_size = 4096;
    void *work_pool = malloc(pool_size);
    if (!work_pool) {
        fclose(fp);
        return false;
    }

    jpeg_io_device_t io_dev = {
        .fp = fp,
        .out_buf = dst_buf,
        .target_w = dst_w,
        .target_h = dst_h
    };

    JDEC jdec;
    JRESULT res = jd_prepare(&jdec, jpeg_input_func, work_pool, pool_size, &io_dev);
    if (res != JDR_OK) {
        ESP_LOGE(TAG, "❌ TJpgDec 准备失败: %d (文件: %s)", res, filepath);
        free(work_pool);
        fclose(fp);
        return false;
    }

    res = jd_decomp(&jdec, jpeg_output_func, 0);
    free(work_pool);
    fclose(fp);

    if (res == JDR_OK) {
        ESP_LOGI(TAG, "🖼️ [TF卡照片] 成功解码并呈现图片: %s (%dx%d)", filepath, jdec.width, jdec.height);
        return true;
    } else {
        ESP_LOGE(TAG, "❌ TJpgDec 解码失败: %d", res);
        return false;
    }
}

int file_reader_get_photo_count(void)
{
    return s_photo_count;
}

bool file_reader_get_photo_path(int index, char *out_path, size_t max_len)
{
    if (s_photo_count > 0 && index < s_photo_count) {
        strncpy(out_path, s_photo_names[index], max_len);
        return true;
    }
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

    // 彻底净化文本：消除 \r、合并连续空行、修剪前后空白
    sanitize_novel_text(out_text_buf);

    ESP_LOGI(TAG, "📖 [TF卡小说] 加载第 %d / %d 页 (读取 %d 字节)", page_index + 1, total_pages, (int)strlen(out_text_buf));
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
