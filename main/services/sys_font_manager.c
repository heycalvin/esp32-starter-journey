#include "sys_font_manager.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "bsp_sdcard.h"
#include "bsp_lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "FONT_MGR";

LV_FONT_DECLARE(font_chinese_14);

static bool s_has_ft_font = false;
static lv_font_t *s_font_14 = NULL;
static lv_font_t *s_font_16 = NULL;
static lv_font_t *s_font_20 = NULL;

esp_err_t sys_font_manager_init(void)
{
    bsp_lvgl_port_lock(0);

#if LV_USE_FREETYPE
    // LVGL v9 的 lv_init() 已经自动初始化了 FreeType 全局上下文
    // 检查 TF 卡中的中文字体文件
    if (bsp_sdcard_is_mounted()) {
        const char *candidate_paths[] = {
            "/sdcard/fonts/chinese.ttf",
            "/sdcard/fonts/simsun.ttf",
            "/sdcard/fonts/font.ttf",
            "/sdcard/chinese.ttf",
            "/sdcard/font.ttf"
        };

        char matched_font_path[280] = {0};
        for (size_t i = 0; i < sizeof(candidate_paths) / sizeof(candidate_paths[0]); i++) {
            struct stat st;
            if (stat(candidate_paths[i], &st) == 0 && st.st_size > 0) {
                strncpy(matched_font_path, candidate_paths[i], sizeof(matched_font_path) - 1);
                break;
            }
        }

        // 若未在候选路径中找到，扫描 /sdcard/fonts 目录
        if (matched_font_path[0] == '\0') {
            DIR *dir = opendir("/sdcard/fonts");
            if (dir) {
                struct dirent *entry;
                while ((entry = readdir(dir)) != NULL) {
                    if (strstr(entry->d_name, ".ttf") || strstr(entry->d_name, ".otf")) {
                        snprintf(matched_font_path, sizeof(matched_font_path), "/sdcard/fonts/%s", entry->d_name);
                        break;
                    }
                }
                closedir(dir);
            }
        }

        if (matched_font_path[0] != '\0') {
            ESP_LOGI(TAG, "📖 发现 TF 卡中文字体: [%s]，正在加载 FreeType 矢量字模...", matched_font_path);
            s_font_14 = lv_freetype_font_create(matched_font_path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 14, LV_FREETYPE_FONT_STYLE_NORMAL);
            s_font_16 = lv_freetype_font_create(matched_font_path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 16, LV_FREETYPE_FONT_STYLE_NORMAL);
            s_font_20 = lv_freetype_font_create(matched_font_path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 20, LV_FREETYPE_FONT_STYLE_NORMAL);

            if (s_font_14 || s_font_16 || s_font_20) {
                s_has_ft_font = true;
                ESP_LOGI(TAG, "🎉 [FreeType] TF 卡矢量中文字体成功挂载！");
            }
        }
    }
#endif

    bsp_lvgl_port_unlock();

    ESP_LOGI(TAG, "🀄 [内置字库] 高清中文 14px 4BPP 抗锯齿字库已就绪 (100%% 覆盖中控台与小说演示)");
    return ESP_OK;
}

bool sys_font_manager_has_chinese_font(void)
{
    return true;
}

const lv_font_t *sys_font_manager_get_font(int size)
{
    if (s_has_ft_font) {
        if (size <= 14 && s_font_14) return s_font_14;
        if (size <= 16 && s_font_16) return s_font_16;
        if (size >= 18 && s_font_20) return s_font_20;
    }

    // 默认内置全汉字高清抗锯齿中文字库
    return &font_chinese_14;
}
