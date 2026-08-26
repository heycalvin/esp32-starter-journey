#include "sys_font_manager.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "FONT_MGR";

LV_FONT_DECLARE(font_chinese_14);

static lv_font_t s_active_chinese_font;
static bool s_font_initialized = false;

esp_err_t sys_font_manager_init(void)
{
    // 将 Flash 只读段的 font_chinese_14 头部元数据浅拷贝到 RAM 中，安全配置 fallback
    memcpy(&s_active_chinese_font, &font_chinese_14, sizeof(lv_font_t));
    s_active_chinese_font.fallback = &lv_font_montserrat_14;
    s_font_initialized = true;

    ESP_LOGI(TAG, "🀄 [全量中文字库] 3500+ 全汉字 4BPP 原生抗锯齿字库已就绪 (已绑定 Symbol 回退)");
    return ESP_OK;
}

bool sys_font_manager_has_chinese_font(void)
{
    return true;
}

const lv_font_t *sys_font_manager_get_font(int size)
{
    if (s_font_initialized) {
        return &s_active_chinese_font;
    }
    return &font_chinese_14;
}
