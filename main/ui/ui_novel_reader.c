#include "ui_novel_reader.h"
#include <stdio.h>
#include <string.h>
#include "file_reader.h"
#include "sys_font_manager.h"
#include "bsp_lvgl_port.h"
#include "esp_log.h"

static const char *TAG __attribute__((unused)) = "UI_NOVEL";

// 当前小说阅读器状态
static int s_current_page = 0;
static int s_total_pages = 1;
static int s_theme_idx = 0; // 0: 暗黑极客, 1: 羊皮复古, 2: 墨蓝科技
static bool s_bars_visible = true;

// Tab 4 预览界面控件
static lv_obj_t *s_tab_text_label = NULL;
static lv_obj_t *s_tab_page_label = NULL;
static lv_obj_t *s_tab_card = NULL;

// 全屏沉浸阅读器控件
static lv_obj_t *s_fs_win = NULL;
static lv_obj_t *s_fs_text_label = NULL;
static lv_obj_t *s_fs_top_bar = NULL;
static lv_obj_t *s_fs_bottom_bar = NULL;
static lv_obj_t *s_fs_page_label = NULL;
static lv_obj_t *s_fs_bar_progress = NULL;
static lv_obj_t *s_fs_title_label = NULL;

// 弹窗控件
static lv_obj_t *s_modal_jump = NULL;
static lv_obj_t *s_slider_jump = NULL;
static lv_obj_t *s_label_jump_val = NULL;
static lv_obj_t *s_modal_trans = NULL;

// 主题色彩定义
typedef struct {
    uint32_t bg_color;
    uint32_t card_color;
    uint32_t text_color;
    uint32_t accent_color;
} novel_theme_t;

static const novel_theme_t s_themes[] = {
    { 0x0A0F1D, 0x1E293B, 0xF1F5F9, 0x38BDF8 }, // 0: 宇宙暗黑
    { 0x1C1917, 0x292524, 0xFDE68A, 0xF59E0B }, // 1: 羊皮复古
    { 0x031B33, 0x0B2A4A, 0xE0F2FE, 0x06B6D4 }, // 2: 科技墨蓝
};

static void apply_current_theme(void)
{
    const novel_theme_t *t = &s_themes[s_theme_idx];
    const lv_font_t *font = sys_font_manager_get_font(14);

    if (s_tab_card) {
        lv_obj_set_style_bg_color(s_tab_card, lv_color_hex(t->card_color), 0);
        if (s_tab_text_label) {
            lv_obj_set_style_text_color(s_tab_text_label, lv_color_hex(t->text_color), 0);
            lv_obj_set_style_text_font(s_tab_text_label, font, 0);
        }
    }

    if (s_fs_win) {
        lv_obj_set_style_bg_color(s_fs_win, lv_color_hex(t->bg_color), 0);
        if (s_fs_text_label) {
            lv_obj_set_style_text_color(s_fs_text_label, lv_color_hex(t->text_color), 0);
            lv_obj_set_style_text_font(s_fs_text_label, font, 0);
        }
    }
}

static void load_and_refresh_page(int page_idx)
{
    if (page_idx < 0) page_idx = 0;
    if (s_total_pages > 0 && page_idx >= s_total_pages) page_idx = s_total_pages - 1;
    s_current_page = page_idx;

    char buf[400];
    file_reader_load_novel_page(s_current_page, buf, sizeof(buf), &s_total_pages);

    // 1. 更新 Tab 4 预览组件
    if (s_tab_text_label) {
        lv_label_set_text(s_tab_text_label, buf);
        lv_obj_set_style_text_color(s_tab_text_label, lv_color_hex(s_themes[s_theme_idx].text_color), 0);
        lv_obj_set_style_text_font(s_tab_text_label, sys_font_manager_get_font(14), 0);
    }
    if (s_tab_page_label) {
        char page_str[32];
        snprintf(page_str, sizeof(page_str), "Page %d / %d", s_current_page + 1, s_total_pages);
        lv_label_set_text(s_tab_page_label, page_str);
    }

    // 2. 更新全屏组件
    if (s_fs_text_label) {
        lv_label_set_text(s_fs_text_label, buf);
        lv_obj_set_style_text_color(s_fs_text_label, lv_color_hex(s_themes[s_theme_idx].text_color), 0);
        lv_obj_set_style_text_font(s_fs_text_label, sys_font_manager_get_font(14), 0);
    }
    if (s_fs_title_label) {
        char title_buf[32];
        snprintf(title_buf, sizeof(title_buf), "Novel #%d", s_current_page + 1);
        lv_label_set_text(s_fs_title_label, title_buf);
    }
    if (s_fs_page_label) {
        int pct = (s_total_pages > 1) ? ((s_current_page * 100) / (s_total_pages - 1)) : 100;
        char fs_page_str[48];
        snprintf(fs_page_str, sizeof(fs_page_str), "Page %d/%d (%d%%)", s_current_page + 1, s_total_pages, pct);
        lv_label_set_text(s_fs_page_label, fs_page_str);
    }
    if (s_fs_bar_progress && s_total_pages > 1) {
        int pct = (s_current_page * 100) / (s_total_pages - 1);
        lv_bar_set_value(s_fs_bar_progress, pct, LV_ANIM_OFF);
    }
}

/* =========================================================================
 * 🔘 交互事件响应
 * ========================================================================= */

// 翻上一页
static void on_prev_clicked(lv_event_t *e)
{
    if (s_current_page > 0) {
        load_and_refresh_page(s_current_page - 1);
    }
}

// 翻下一页
static void on_next_clicked(lv_event_t *e)
{
    if (s_current_page < s_total_pages - 1) {
        load_and_refresh_page(s_current_page + 1);
    }
}

// 切换配色主题
static void on_theme_toggle_clicked(lv_event_t *e)
{
    s_theme_idx = (s_theme_idx + 1) % 3;
    apply_current_theme();
}

// 触屏三区域智能翻页与工具栏唤起
static void on_fs_content_clicked(lv_event_t *e)
{
    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;

    lv_point_t pt;
    lv_indev_get_point(indev, &pt);

    // 屏幕宽度 240：
    // 左侧 0~70: 上一页
    // 右侧 170~240: 下一页
    // 中间 71~169: 切换工具栏显隐
    if (pt.x < 70) {
        if (s_current_page > 0) {
            load_and_refresh_page(s_current_page - 1);
        }
    } else if (pt.x > 170) {
        if (s_current_page < s_total_pages - 1) {
            load_and_refresh_page(s_current_page + 1);
        }
    } else {
        s_bars_visible = !s_bars_visible;
        if (s_bars_visible) {
            lv_obj_clear_flag(s_fs_top_bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_fs_bottom_bar, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_fs_top_bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_fs_bottom_bar, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// 保存书签
static void on_bookmark_save_clicked(lv_event_t *e)
{
    file_reader_set_bookmark(s_current_page);
    lv_obj_t *msgbox = lv_msgbox_create(lv_screen_active());
    lv_msgbox_add_title(msgbox, LV_SYMBOL_OK " 书签已保存");
    char toast[64];
    snprintf(toast, sizeof(toast), "当前第 %d 页已存入 NVS 记忆", s_current_page + 1);
    lv_msgbox_add_text(msgbox, toast);
    lv_obj_set_style_text_font(msgbox, sys_font_manager_get_font(14), 0);
    lv_msgbox_add_close_button(msgbox);
}

// 加载书签
static void on_bookmark_load_clicked(lv_event_t *e)
{
    int saved = file_reader_get_bookmark();
    load_and_refresh_page(saved);
}

// 查词与翻译工具弹窗
static void on_trans_clicked(lv_event_t *e)
{
    if (s_modal_trans) {
        lv_obj_del(s_modal_trans);
        s_modal_trans = NULL;
    }

    s_modal_trans = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_modal_trans, 220, 200);
    lv_obj_center(s_modal_trans);
    lv_obj_set_style_bg_color(s_modal_trans, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(s_modal_trans, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_border_width(s_modal_trans, 2, 0);
    lv_obj_set_style_radius(s_modal_trans, 12, 0);

    lv_obj_t *title = lv_label_create(s_modal_trans);
    lv_label_set_text(title, LV_SYMBOL_EDIT " 词典与术语");
    lv_obj_set_style_text_color(title, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_text_font(title, sys_font_manager_get_font(14), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *body = lv_label_create(s_modal_trans);
    lv_label_set_text(body,
                      "宇宙科幻核心词汇:\n"
                      "Universe: 宇宙\n"
                      "Antenna: 射电天线\n"
                      "Signal: 脉冲信号\n"
                      "Civilization: 文明\n"
                      "Starship: 恒星飞船");
    lv_obj_set_style_text_color(body, lv_color_hex(0xE2E8F0), 0);
    lv_obj_set_style_text_font(body, sys_font_manager_get_font(14), 0);
    lv_obj_align(body, LV_ALIGN_CENTER, 0, 5);

    lv_obj_t *btn_close = lv_button_create(s_modal_trans);
    lv_obj_set_size(btn_close, 100, 32);
    lv_obj_align(btn_close, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0x2563EB), 0);
    lv_obj_add_event_cb(btn_close, (lv_event_cb_t)lv_obj_delete_async, LV_EVENT_CLICKED, s_modal_trans);
    lv_obj_t *lbl_c = lv_label_create(btn_close);
    lv_label_set_text(lbl_c, "关闭");
    lv_obj_set_style_text_font(lbl_c, sys_font_manager_get_font(14), 0);
    lv_obj_center(lbl_c);
}

// 快速跳转弹窗
static void on_slider_changed(lv_event_t *e)
{
    if (s_slider_jump && s_label_jump_val) {
        int val = (int)lv_slider_get_value(s_slider_jump);
        char buf[64];
        snprintf(buf, sizeof(buf), "跳转至: 第 %d / %d 页", val + 1, s_total_pages);
        lv_label_set_text(s_label_jump_val, buf);
    }
}

static void on_jump_confirm(lv_event_t *e)
{
    if (s_slider_jump) {
        int val = (int)lv_slider_get_value(s_slider_jump);
        load_and_refresh_page(val);
    }
    if (s_modal_jump) {
        lv_obj_del(s_modal_jump);
        s_modal_jump = NULL;
    }
}

static void on_jump_clicked(lv_event_t *e)
{
    if (s_modal_jump) {
        lv_obj_del(s_modal_jump);
        s_modal_jump = NULL;
    }

    s_modal_jump = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_modal_jump, 220, 180);
    lv_obj_center(s_modal_jump);
    lv_obj_set_style_bg_color(s_modal_jump, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(s_modal_jump, lv_color_hex(0x10B981), 0);
    lv_obj_set_style_border_width(s_modal_jump, 2, 0);
    lv_obj_set_style_radius(s_modal_jump, 12, 0);

    lv_obj_t *title = lv_label_create(s_modal_jump);
    lv_label_set_text(title, LV_SYMBOL_RIGHT " 快速章节跳转");
    lv_obj_set_style_text_color(title, lv_color_hex(0x10B981), 0);
    lv_obj_set_style_text_font(title, sys_font_manager_get_font(14), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    s_label_jump_val = lv_label_create(s_modal_jump);
    char buf[64];
    snprintf(buf, sizeof(buf), "跳转至: 第 %d / %d 页", s_current_page + 1, s_total_pages);
    lv_label_set_text(s_label_jump_val, buf);
    lv_obj_set_style_text_color(s_label_jump_val, lv_color_hex(0xE2E8F0), 0);
    lv_obj_set_style_text_font(s_label_jump_val, sys_font_manager_get_font(14), 0);
    lv_obj_align(s_label_jump_val, LV_ALIGN_CENTER, 0, -20);

    s_slider_jump = lv_slider_create(s_modal_jump);
    lv_obj_set_size(s_slider_jump, 180, 12);
    lv_obj_align(s_slider_jump, LV_ALIGN_CENTER, 0, 10);
    lv_slider_set_range(s_slider_jump, 0, s_total_pages > 1 ? s_total_pages - 1 : 1);
    lv_slider_set_value(s_slider_jump, s_current_page, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_slider_jump, on_slider_changed, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *btn_ok = lv_button_create(s_modal_jump);
    lv_obj_set_size(btn_ok, 80, 32);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_LEFT, 10, -4);
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0x10B981), 0);
    lv_obj_add_event_cb(btn_ok, on_jump_confirm, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_ok = lv_label_create(btn_ok);
    lv_label_set_text(lbl_ok, "确认");
    lv_obj_set_style_text_font(lbl_ok, sys_font_manager_get_font(14), 0);
    lv_obj_center(lbl_ok);

    lv_obj_t *btn_cancel = lv_button_create(s_modal_jump);
    lv_obj_set_size(btn_cancel, 80, 32);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_RIGHT, -10, -4);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x475569), 0);
    lv_obj_add_event_cb(btn_cancel, (lv_event_cb_t)lv_obj_delete_async, LV_EVENT_CLICKED, s_modal_jump);
    lv_obj_t *lbl_can = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_can, "取消");
    lv_obj_set_style_text_font(lbl_can, sys_font_manager_get_font(14), 0);
    lv_obj_center(lbl_can);
}

// 打开全屏沉浸阅读器
void ui_novel_reader_open_fullscreen(void)
{
    if (s_fs_win) {
        lv_obj_clear_flag(s_fs_win, LV_OBJ_FLAG_HIDDEN);
        load_and_refresh_page(s_current_page);
        return;
    }

    s_fs_win = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_fs_win, 240, 280);
    lv_obj_align(s_fs_win, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_radius(s_fs_win, 0, 0);
    lv_obj_set_style_pad_all(s_fs_win, 6, 0);
    lv_obj_add_event_cb(s_fs_win, on_fs_content_clicked, LV_EVENT_CLICKED, NULL);

    // 正文文本区域 (舒适排版与行间距)
    s_fs_text_label = lv_label_create(s_fs_win);
    lv_obj_set_width(s_fs_text_label, 224);
    lv_label_set_long_mode(s_fs_text_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(s_fs_text_label, 4, 0);
    lv_obj_align(s_fs_text_label, LV_ALIGN_TOP_MID, 0, 36);

    // 浮动顶栏 (Top Floating Bar)
    s_fs_top_bar = lv_obj_create(s_fs_win);
    lv_obj_set_size(s_fs_top_bar, 232, 34);
    lv_obj_align(s_fs_top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s_fs_top_bar, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_radius(s_fs_top_bar, 8, 0);
    lv_obj_set_style_pad_all(s_fs_top_bar, 2, 0);

    s_fs_title_label = lv_label_create(s_fs_top_bar);
    lv_label_set_text(s_fs_title_label, "三体 · 连载");
    lv_obj_set_style_text_color(s_fs_title_label, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_text_font(s_fs_title_label, sys_font_manager_get_font(14), 0);
    lv_obj_align(s_fs_title_label, LV_ALIGN_LEFT_MID, 6, 0);

    lv_obj_t *btn_theme = lv_button_create(s_fs_top_bar);
    lv_obj_set_size(btn_theme, 36, 26);
    lv_obj_align(btn_theme, LV_ALIGN_RIGHT_MID, -44, 0);
    lv_obj_set_style_bg_color(btn_theme, lv_color_hex(0x334155), 0);
    lv_obj_add_event_cb(btn_theme, on_theme_toggle_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_th = lv_label_create(btn_theme);
    lv_label_set_text(lbl_th, LV_SYMBOL_EYE_OPEN);
    lv_obj_center(lbl_th);

    lv_obj_t *btn_close = lv_button_create(s_fs_top_bar);
    lv_obj_set_size(btn_close, 36, 26);
    lv_obj_align(btn_close, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0xDC2626), 0);
    lv_obj_add_event_cb(btn_close, (lv_event_cb_t)lv_obj_delete_async, LV_EVENT_CLICKED, s_fs_win);
    lv_obj_t *lbl_x = lv_label_create(btn_close);
    lv_label_set_text(lbl_x, LV_SYMBOL_CLOSE);
    lv_obj_center(lbl_x);

    // 浮动底栏 (Bottom Floating Bar)
    s_fs_bottom_bar = lv_obj_create(s_fs_win);
    lv_obj_set_size(s_fs_bottom_bar, 232, 44);
    lv_obj_align(s_fs_bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(s_fs_bottom_bar, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_radius(s_fs_bottom_bar, 8, 0);
    lv_obj_set_style_pad_all(s_fs_bottom_bar, 2, 0);

    // 进度条
    s_fs_bar_progress = lv_bar_create(s_fs_bottom_bar);
    lv_obj_set_size(s_fs_bar_progress, 220, 4);
    lv_obj_align(s_fs_bar_progress, LV_ALIGN_TOP_MID, 0, 1);
    lv_bar_set_range(s_fs_bar_progress, 0, 100);
    lv_obj_set_style_bg_color(s_fs_bar_progress, lv_color_hex(0x38BDF8), LV_PART_INDICATOR);

    // 上一页
    lv_obj_t *btn_p = lv_button_create(s_fs_bottom_bar);
    lv_obj_set_size(btn_p, 36, 28);
    lv_obj_align(btn_p, LV_ALIGN_BOTTOM_LEFT, 2, -2);
    lv_obj_set_style_bg_color(btn_p, lv_color_hex(0x334155), 0);
    lv_obj_add_event_cb(btn_p, on_prev_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lp = lv_label_create(btn_p);
    lv_label_set_text(lp, LV_SYMBOL_PREV);
    lv_obj_center(lp);

    // 存书签
    lv_obj_t *btn_bm = lv_button_create(s_fs_bottom_bar);
    lv_obj_set_size(btn_bm, 36, 28);
    lv_obj_align(btn_bm, LV_ALIGN_BOTTOM_LEFT, 42, -2);
    lv_obj_set_style_bg_color(btn_bm, lv_color_hex(0xF59E0B), 0);
    lv_obj_add_event_cb(btn_bm, on_bookmark_save_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbm = lv_label_create(btn_bm);
    lv_label_set_text(lbm, LV_SYMBOL_SAVE);
    lv_obj_center(lbm);

    // 中间页码
    s_fs_page_label = lv_label_create(s_fs_bottom_bar);
    lv_label_set_text(s_fs_page_label, "Page 1 / 1");
    lv_obj_set_style_text_color(s_fs_page_label, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(s_fs_page_label, &lv_font_montserrat_14, 0);
    lv_obj_align(s_fs_page_label, LV_ALIGN_BOTTOM_MID, 16, -6);

    // 查词
    lv_obj_t *btn_tr = lv_button_create(s_fs_bottom_bar);
    lv_obj_set_size(btn_tr, 36, 28);
    lv_obj_align(btn_tr, LV_ALIGN_BOTTOM_RIGHT, -42, -2);
    lv_obj_set_style_bg_color(btn_tr, lv_color_hex(0x2563EB), 0);
    lv_obj_add_event_cb(btn_tr, on_trans_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ltr = lv_label_create(btn_tr);
    lv_label_set_text(ltr, LV_SYMBOL_EDIT);
    lv_obj_center(ltr);

    // 下一页
    lv_obj_t *btn_n = lv_button_create(s_fs_bottom_bar);
    lv_obj_set_size(btn_n, 36, 28);
    lv_obj_align(btn_n, LV_ALIGN_BOTTOM_RIGHT, -2, -2);
    lv_obj_set_style_bg_color(btn_n, lv_color_hex(0x334155), 0);
    lv_obj_add_event_cb(btn_n, on_next_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ln = lv_label_create(btn_n);
    lv_label_set_text(ln, LV_SYMBOL_NEXT);
    lv_obj_center(ln);

    apply_current_theme();
    load_and_refresh_page(s_current_page);
}

void ui_novel_reader_init(lv_obj_t *parent_tab)
{
    if (!parent_tab) return;

    // 1. 顶部标题
    lv_obj_t *title = lv_label_create(parent_tab);
    lv_label_set_text(title, "E-BOOK NOVEL READER");
    lv_obj_set_style_text_color(title, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    // 2. 小说阅读主卡片 (包含正文预览)
    s_tab_card = lv_obj_create(parent_tab);
    lv_obj_set_size(s_tab_card, 222, 100);
    lv_obj_align(s_tab_card, LV_ALIGN_CENTER, 0, -22);
    lv_obj_set_style_border_color(s_tab_card, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(s_tab_card, 10, 0);
    lv_obj_set_style_pad_all(s_tab_card, 6, 0);

    s_tab_text_label = lv_label_create(s_tab_card);
    lv_obj_set_width(s_tab_text_label, 204);
    lv_label_set_long_mode(s_tab_text_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(s_tab_text_label, 3, 0);
    lv_obj_align(s_tab_text_label, LV_ALIGN_TOP_LEFT, 0, 0);

    // 3. 全屏阅读触发大按钮
    lv_obj_t *btn_fullscreen = lv_button_create(parent_tab);
    lv_obj_set_size(btn_fullscreen, 222, 34);
    lv_obj_align(btn_fullscreen, LV_ALIGN_CENTER, 0, 48);
    lv_obj_set_style_bg_color(btn_fullscreen, lv_color_hex(0x2563EB), 0);
    lv_obj_set_style_radius(btn_fullscreen, 8, 0);
    lv_obj_add_event_cb(btn_fullscreen, (lv_event_cb_t)ui_novel_reader_open_fullscreen, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_fs = lv_label_create(btn_fullscreen);
    lv_label_set_text(lbl_fs, LV_SYMBOL_PLAY " Enter Fullscreen Reading");
    lv_obj_set_style_text_font(lbl_fs, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_fs);

    // 4. 底部快捷工具栏 (上一页、书签、跳转、下一页)
    lv_obj_t *btn_prev = lv_button_create(parent_tab);
    lv_obj_set_size(btn_prev, 46, 30);
    lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_LEFT, 4, -4);
    lv_obj_set_style_bg_color(btn_prev, lv_color_hex(0x334155), 0);
    lv_obj_add_event_cb(btn_prev, on_prev_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lp = lv_label_create(btn_prev);
    lv_label_set_text(lp, LV_SYMBOL_PREV);
    lv_obj_center(lp);

    lv_obj_t *btn_bm = lv_button_create(parent_tab);
    lv_obj_set_size(btn_bm, 50, 30);
    lv_obj_align(btn_bm, LV_ALIGN_BOTTOM_LEFT, 54, -4);
    lv_obj_set_style_bg_color(btn_bm, lv_color_hex(0xF59E0B), 0);
    lv_obj_add_event_cb(btn_bm, on_bookmark_load_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbm = lv_label_create(btn_bm);
    lv_label_set_text(lbm, LV_SYMBOL_SAVE " Mark");
    lv_obj_set_style_text_font(lbm, &lv_font_montserrat_14, 0);
    lv_obj_center(lbm);

    lv_obj_t *btn_jump = lv_button_create(parent_tab);
    lv_obj_set_size(btn_jump, 50, 30);
    lv_obj_align(btn_jump, LV_ALIGN_BOTTOM_RIGHT, -54, -4);
    lv_obj_set_style_bg_color(btn_jump, lv_color_hex(0x10B981), 0);
    lv_obj_add_event_cb(btn_jump, on_jump_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lj = lv_label_create(btn_jump);
    lv_label_set_text(lj, LV_SYMBOL_RIGHT " Jump");
    lv_obj_set_style_text_font(lj, &lv_font_montserrat_14, 0);
    lv_obj_center(lj);

    lv_obj_t *btn_next = lv_button_create(parent_tab);
    lv_obj_set_size(btn_next, 46, 30);
    lv_obj_align(btn_next, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
    lv_obj_set_style_bg_color(btn_next, lv_color_hex(0x334155), 0);
    lv_obj_add_event_cb(btn_next, on_next_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ln = lv_label_create(btn_next);
    lv_label_set_text(ln, LV_SYMBOL_NEXT);
    lv_obj_center(ln);

    // 5. 页码小标签
    s_tab_page_label = lv_label_create(parent_tab);
    lv_label_set_text(s_tab_page_label, "Page 1 / 1");
    lv_obj_set_style_text_color(s_tab_page_label, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(s_tab_page_label, &lv_font_montserrat_14, 0);
    lv_obj_align(s_tab_page_label, LV_ALIGN_CENTER, 0, 72);

    apply_current_theme();
    load_and_refresh_page(0);
}

void ui_novel_reader_refresh_content(void)
{
    apply_current_theme();
    load_and_refresh_page(s_current_page);
}
