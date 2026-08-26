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
static int s_total_pages = 9;
static int s_theme_idx = 0; // 0: 暗黑极客, 1: 羊皮复古, 2: 墨蓝科技
static bool s_bars_visible = true;

// Tab 4 控件
static lv_obj_t *s_tab_text_label = NULL;
static lv_obj_t *s_tab_page_label = NULL;
static lv_obj_t *s_tab_card = NULL;

// 全屏沉浸阅读器控件
static lv_obj_t *s_fs_win = NULL;
static lv_obj_t *s_fs_scroll_cont = NULL;
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
    { 0x0A0F1D, 0x0F172A, 0xF1F5F9, 0x38BDF8 }, // 0: 宇宙暗黑
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
        if (s_fs_scroll_cont) {
            lv_obj_set_style_bg_color(s_fs_scroll_cont, lv_color_hex(t->bg_color), 0);
        }
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

    char buf[1024];
    memset(buf, 0, sizeof(buf));
    file_reader_load_novel_page(s_current_page, buf, sizeof(buf), &s_total_pages);

    const lv_font_t *font = sys_font_manager_get_font(14);

    // 1. 更新 Tab 4 预览组件
    if (s_tab_text_label) {
        lv_label_set_text(s_tab_text_label, buf);
        lv_obj_set_style_text_color(s_tab_text_label, lv_color_hex(s_themes[s_theme_idx].text_color), 0);
        lv_obj_set_style_text_font(s_tab_text_label, font, 0);
    }
    if (s_tab_page_label) {
        char page_str[64];
        snprintf(page_str, sizeof(page_str), "第 %d / %d 页", s_current_page + 1, s_total_pages);
        lv_label_set_text(s_tab_page_label, page_str);
    }

    // 2. 更新全屏组件 (重置滚动条位置至顶部)
    if (s_fs_scroll_cont) {
        lv_obj_scroll_to_y(s_fs_scroll_cont, 0, LV_ANIM_OFF);
    }
    if (s_fs_text_label) {
        lv_label_set_text(s_fs_text_label, buf);
        lv_obj_set_style_text_color(s_fs_text_label, lv_color_hex(s_themes[s_theme_idx].text_color), 0);
        lv_obj_set_style_text_font(s_fs_text_label, font, 0);
    }
    if (s_fs_title_label) {
        char title_buf[64] = "《星海编年史》";
        int ch_count = file_reader_get_chapter_count();
        for (int i = ch_count - 1; i >= 0; i--) {
            const novel_chapter_t *ch = file_reader_get_chapter(i);
            if (ch && s_current_page >= ch->page_index) {
                snprintf(title_buf, sizeof(title_buf), "%s", ch->title);
                break;
            }
        }
        lv_label_set_text(s_fs_title_label, title_buf);
    }
    if (s_fs_page_label) {
        int pct = (s_total_pages > 1) ? ((s_current_page * 100) / (s_total_pages - 1)) : 100;
        char fs_page_str[64];
        snprintf(fs_page_str, sizeof(fs_page_str), "第%d/%d页 (%d%%)", s_current_page + 1, s_total_pages, pct);
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

// 退出全屏阅读器（返回桌面）
static void on_close_fullscreen_clicked(lv_event_t *e)
{
    if (s_fs_win) {
        lv_obj_add_flag(s_fs_win, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "📖 退出全屏小说阅读器，无缝返回中控台主界面");
    }
}

// 通用弹窗安全关闭回调 (杜绝野指针 Panic)
static void on_modal_close_btn_clicked(lv_event_t *e)
{
    lv_obj_t *modal = (lv_obj_t *)lv_event_get_user_data(e);
    if (modal) {
        lv_obj_del(modal);
    }
    if (modal == s_modal_trans) s_modal_trans = NULL;
    if (modal == s_modal_jump) s_modal_jump = NULL;
}

static lv_point_t s_touch_start_point;
static bool s_is_pressing = false;

// 沉浸式阅读器统一触控响应引擎 (支持高精度点击判定与手势滑动)
static void on_fs_touch_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;

    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &s_touch_start_point);
        s_is_pressing = true;
    } else if (code == LV_EVENT_RELEASED) {
        if (!s_is_pressing) return;
        s_is_pressing = false;

        lv_point_t end_pt;
        lv_indev_get_point(indev, &end_pt);

        int dx = (int)end_pt.x - (int)s_touch_start_point.x;
        int dy = (int)end_pt.y - (int)s_touch_start_point.y;
        int abs_dx = (dx < 0) ? -dx : dx;
        int abs_dy = (dy < 0) ? -dy : dy;

        // 1. 水平滑动手势 (滑动距离 > 25px)
        if (abs_dx > 25 && abs_dx > abs_dy) {
            if (dx < 0) {
                // 向左滑 -> 下一页
                if (s_current_page < s_total_pages - 1) load_and_refresh_page(s_current_page + 1);
            } else {
                // 向右滑 -> 上一页
                if (s_current_page > 0) load_and_refresh_page(s_current_page - 1);
            }
            return;
        }

        // 2. 静止轻触点击 (位移 < 20px)
        if (abs_dx < 20 && abs_dy < 20) {
            if (end_pt.x < 70) {
                // 点击左侧 -> 上一页
                if (s_current_page > 0) load_and_refresh_page(s_current_page - 1);
            } else if (end_pt.x > 170) {
                // 点击右侧 -> 下一页
                if (s_current_page < s_total_pages - 1) load_and_refresh_page(s_current_page + 1);
            } else {
                // 点击中间 -> 切换顶底工具栏显隐
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

// 科幻术语表弹窗 (带安全关闭处理)
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
    lv_label_set_text(title, LV_SYMBOL_EDIT " 科幻术语表");
    lv_obj_set_style_text_color(title, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_text_font(title, sys_font_manager_get_font(14), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *body = lv_label_create(s_modal_trans);
    lv_label_set_text(body,
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
    lv_obj_add_event_cb(btn_close, on_modal_close_btn_clicked, LV_EVENT_CLICKED, s_modal_trans);
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
    lv_obj_add_event_cb(btn_cancel, on_modal_close_btn_clicked, LV_EVENT_CLICKED, s_modal_jump);
    lv_obj_t *lbl_can = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_can, "取消");
    lv_obj_set_style_text_font(lbl_can, sys_font_manager_get_font(14), 0);
    lv_obj_center(lbl_can);
}

static lv_obj_t *s_modal_toc = NULL;

static void on_toc_item_clicked(lv_event_t *e)
{
    int page_idx = (int)(intptr_t)lv_event_get_user_data(e);
    load_and_refresh_page(page_idx);
    if (s_modal_toc) {
        lv_obj_del(s_modal_toc);
        s_modal_toc = NULL;
    }
}

static void on_toc_clicked(lv_event_t *e)
{
    if (s_modal_toc) {
        lv_obj_del(s_modal_toc);
        s_modal_toc = NULL;
    }

    const lv_font_t *font_cn = sys_font_manager_get_font(14);
    s_modal_toc = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_modal_toc, 226, 230);
    lv_obj_center(s_modal_toc);
    lv_obj_set_style_bg_color(s_modal_toc, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(s_modal_toc, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_border_width(s_modal_toc, 2, 0);
    lv_obj_set_style_radius(s_modal_toc, 12, 0);
    lv_obj_set_style_pad_all(s_modal_toc, 6, 0);

    // 目录标题与关闭按钮
    lv_obj_t *header = lv_obj_create(s_modal_toc);
    lv_obj_set_size(header, 210, 28);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 2, 0);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "📑 章节目录 (点击跳转)");
    lv_obj_set_style_text_color(title, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_text_font(title, font_cn, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 4, 0);

    lv_obj_t *btn_x = lv_button_create(header);
    lv_obj_set_size(btn_x, 24, 22);
    lv_obj_align(btn_x, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(btn_x, lv_color_hex(0xDC2626), 0);
    lv_obj_add_event_cb(btn_x, on_modal_close_btn_clicked, LV_EVENT_CLICKED, s_modal_toc);
    lv_obj_t *lbl_x = lv_label_create(btn_x);
    lv_label_set_text(lbl_x, LV_SYMBOL_CLOSE);
    lv_obj_center(lbl_x);

    // 章节列表 List (可垂直滚动)
    lv_obj_t *list = lv_list_create(s_modal_toc);
    lv_obj_set_size(list, 212, 180);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 2, 0);

    int count = file_reader_get_chapter_count();
    if (count == 0) {
        lv_obj_t *empty_lbl = lv_label_create(list);
        lv_label_set_text(empty_lbl, "未检测到章节标记\n支持直接拖动滑块跳转");
        lv_obj_set_style_text_font(empty_lbl, font_cn, 0);
        lv_obj_set_style_text_color(empty_lbl, lv_color_hex(0x94A3B8), 0);
    } else {
        for (int i = 0; i < count; i++) {
            const novel_chapter_t *ch = file_reader_get_chapter(i);
            if (!ch) continue;
            
            bool is_current = (s_current_page >= ch->page_index &&
                (i == count - 1 || s_current_page < file_reader_get_chapter(i+1)->page_index));

            const char *icon = is_current ? LV_SYMBOL_PLAY : LV_SYMBOL_FILE;
            lv_obj_t *btn = lv_list_add_button(list, icon, ch->title);
            lv_obj_set_size(btn, 204, 34);
            lv_obj_set_style_text_font(btn, font_cn, 0);
            lv_obj_set_style_radius(btn, 6, 0);
            lv_obj_set_style_pad_all(btn, 4, 0);

            if (is_current) {
                // 当前阅读项：明亮天蓝底 + 纯白高亮文字 + 青色发光边框
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x0284C7), 0);
                lv_obj_set_style_border_color(btn, lv_color_hex(0x38BDF8), 0);
                lv_obj_set_style_border_width(btn, 2, 0);
                lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
            } else {
                // 未选定项：深灰底色 + 纯白高亮文字 + 浅灰边框 (对比度拉满)
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E293B), 0);
                lv_obj_set_style_border_color(btn, lv_color_hex(0x475569), 0);
                lv_obj_set_style_border_width(btn, 1, 0);
                lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
            }

            // 递归确保按钮内每一个子 label 文字颜色均为高对比度纯白
            uint32_t child_cnt = lv_obj_get_child_count(btn);
            for (uint32_t c = 0; c < child_cnt; c++) {
                lv_obj_t *child = lv_obj_get_child(btn, c);
                lv_obj_set_style_text_font(child, font_cn, 0);
                lv_obj_set_style_text_color(child, is_current ? lv_color_hex(0xFFFFFF) : lv_color_hex(0xF8FAFC), 0);
            }

            lv_obj_add_event_cb(btn, on_toc_item_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)ch->page_index);
        }
    }
}

// 打开全屏沉浸阅读器
void ui_novel_reader_open_fullscreen(void)
{
    const lv_font_t *font_cn = sys_font_manager_get_font(14);

    if (s_fs_win) {
        lv_obj_clear_flag(s_fs_win, LV_OBJ_FLAG_HIDDEN);
        load_and_refresh_page(s_current_page);
        return;
    }

    s_fs_win = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_fs_win, 240, 280);
    lv_obj_align(s_fs_win, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_fs_win, lv_color_hex(0x0A0F1D), 0);
    lv_obj_set_style_radius(s_fs_win, 0, 0);
    lv_obj_set_style_pad_all(s_fs_win, 0, 0);
    lv_obj_add_event_cb(s_fs_win, on_fs_touch_event, LV_EVENT_ALL, NULL);

    // 正文垂直平滑可滚动容器 (全屏尺寸 240x280，上下预留边距，支持自然上下滑动与弹性滚动)
    s_fs_scroll_cont = lv_obj_create(s_fs_win);
    lv_obj_set_size(s_fs_scroll_cont, 240, 280);
    lv_obj_align(s_fs_scroll_cont, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_fs_scroll_cont, lv_color_hex(0x0A0F1D), 0);
    lv_obj_set_style_border_width(s_fs_scroll_cont, 0, 0);
    lv_obj_set_style_radius(s_fs_scroll_cont, 0, 0);
    lv_obj_set_style_pad_top(s_fs_scroll_cont, 36, 0);
    lv_obj_set_style_pad_bottom(s_fs_scroll_cont, 40, 0);
    lv_obj_set_style_pad_hor(s_fs_scroll_cont, 8, 0);
    lv_obj_set_scroll_dir(s_fs_scroll_cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_fs_scroll_cont, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_event_cb(s_fs_scroll_cont, on_fs_touch_event, LV_EVENT_ALL, NULL);

    // 正文文本区域 (舒适排版与行间距，开启点击与事件冒泡)
    s_fs_text_label = lv_label_create(s_fs_scroll_cont);
    lv_obj_set_width(s_fs_text_label, 224);
    lv_label_set_long_mode(s_fs_text_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(s_fs_text_label, 5, 0);
    lv_obj_set_style_text_font(s_fs_text_label, font_cn, 0);
    lv_obj_align(s_fs_text_label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_flag(s_fs_text_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_fs_text_label, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(s_fs_text_label, on_fs_touch_event, LV_EVENT_ALL, NULL);

    // 浮动顶栏 (Top Floating Bar)
    s_fs_top_bar = lv_obj_create(s_fs_win);
    lv_obj_set_size(s_fs_top_bar, 236, 30);
    lv_obj_align(s_fs_top_bar, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(s_fs_top_bar, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(s_fs_top_bar, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(s_fs_top_bar, 1, 0);
    lv_obj_set_style_radius(s_fs_top_bar, 6, 0);
    lv_obj_set_style_pad_all(s_fs_top_bar, 2, 0);

    // 返回按钮 (大号醒目)
    lv_obj_t *btn_back = lv_button_create(s_fs_top_bar);
    lv_obj_set_size(btn_back, 64, 24);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x2563EB), 0);
    lv_obj_add_event_cb(btn_back, on_close_fullscreen_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_bk = lv_label_create(btn_back);
    lv_label_set_text(lbl_bk, LV_SYMBOL_LEFT " 返回");
    lv_obj_set_style_text_font(lbl_bk, font_cn, 0);
    lv_obj_center(lbl_bk);

    s_fs_title_label = lv_label_create(s_fs_top_bar);
    lv_label_set_text(s_fs_title_label, "《星海编年史》");
    lv_obj_set_style_text_color(s_fs_title_label, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_text_font(s_fs_title_label, font_cn, 0);
    lv_obj_align(s_fs_title_label, LV_ALIGN_CENTER, 10, 0);

    // 主题切换按钮
    lv_obj_t *btn_theme = lv_button_create(s_fs_top_bar);
    lv_obj_set_size(btn_theme, 30, 24);
    lv_obj_align(btn_theme, LV_ALIGN_RIGHT_MID, -34, 0);
    lv_obj_set_style_bg_color(btn_theme, lv_color_hex(0x334155), 0);
    lv_obj_add_event_cb(btn_theme, on_theme_toggle_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_th = lv_label_create(btn_theme);
    lv_label_set_text(lbl_th, LV_SYMBOL_EYE_OPEN);
    lv_obj_center(lbl_th);

    // 退出关闭按钮
    lv_obj_t *btn_close = lv_button_create(s_fs_top_bar);
    lv_obj_set_size(btn_close, 30, 24);
    lv_obj_align(btn_close, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0xDC2626), 0);
    lv_obj_add_event_cb(btn_close, on_close_fullscreen_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_x = lv_label_create(btn_close);
    lv_label_set_text(lbl_x, LV_SYMBOL_CLOSE);
    lv_obj_center(lbl_x);

    // 浮动底栏 (Bottom Floating Bar)
    s_fs_bottom_bar = lv_obj_create(s_fs_win);
    lv_obj_set_size(s_fs_bottom_bar, 236, 36);
    lv_obj_align(s_fs_bottom_bar, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_bg_color(s_fs_bottom_bar, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(s_fs_bottom_bar, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(s_fs_bottom_bar, 1, 0);
    lv_obj_set_style_radius(s_fs_bottom_bar, 6, 0);
    lv_obj_set_style_pad_all(s_fs_bottom_bar, 2, 0);

    // 进度条
    s_fs_bar_progress = lv_bar_create(s_fs_bottom_bar);
    lv_obj_set_size(s_fs_bar_progress, 226, 3);
    lv_obj_align(s_fs_bar_progress, LV_ALIGN_TOP_MID, 0, 0);
    lv_bar_set_range(s_fs_bar_progress, 0, 100);
    lv_obj_set_style_bg_color(s_fs_bar_progress, lv_color_hex(0x38BDF8), LV_PART_INDICATOR);

    // 1. 上一页
    lv_obj_t *btn_p = lv_button_create(s_fs_bottom_bar);
    lv_obj_set_size(btn_p, 30, 24);
    lv_obj_align(btn_p, LV_ALIGN_BOTTOM_LEFT, 2, -1);
    lv_obj_set_style_bg_color(btn_p, lv_color_hex(0x334155), 0);
    lv_obj_add_event_cb(btn_p, on_prev_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lp = lv_label_create(btn_p);
    lv_label_set_text(lp, LV_SYMBOL_PREV);
    lv_obj_center(lp);

    // 2. 📑 目录按钮 (智能章节列表)
    lv_obj_t *btn_toc = lv_button_create(s_fs_bottom_bar);
    lv_obj_set_size(btn_toc, 42, 24);
    lv_obj_align(btn_toc, LV_ALIGN_BOTTOM_LEFT, 35, -1);
    lv_obj_set_style_bg_color(btn_toc, lv_color_hex(0x0284C7), 0);
    lv_obj_add_event_cb(btn_toc, on_toc_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ltoc = lv_label_create(btn_toc);
    lv_label_set_text(ltoc, "目录");
    lv_obj_set_style_text_font(ltoc, font_cn, 0);
    lv_obj_center(ltoc);

    // 3. 存书签
    lv_obj_t *btn_bm = lv_button_create(s_fs_bottom_bar);
    lv_obj_set_size(btn_bm, 30, 24);
    lv_obj_align(btn_bm, LV_ALIGN_BOTTOM_LEFT, 80, -1);
    lv_obj_set_style_bg_color(btn_bm, lv_color_hex(0xF59E0B), 0);
    lv_obj_add_event_cb(btn_bm, on_bookmark_save_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbm = lv_label_create(btn_bm);
    lv_label_set_text(lbm, LV_SYMBOL_SAVE);
    lv_obj_center(lbm);

    // 4. 滑块跳转
    lv_obj_t *btn_jump = lv_button_create(s_fs_bottom_bar);
    lv_obj_set_size(btn_jump, 30, 24);
    lv_obj_align(btn_jump, LV_ALIGN_BOTTOM_RIGHT, -35, -1);
    lv_obj_set_style_bg_color(btn_jump, lv_color_hex(0x059669), 0);
    lv_obj_add_event_cb(btn_jump, on_jump_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ljump = lv_label_create(btn_jump);
    lv_label_set_text(ljump, LV_SYMBOL_RIGHT);
    lv_obj_center(ljump);

    // 5. 下一页
    lv_obj_t *btn_n = lv_button_create(s_fs_bottom_bar);
    lv_obj_set_size(btn_n, 30, 24);
    lv_obj_align(btn_n, LV_ALIGN_BOTTOM_RIGHT, -2, -1);
    lv_obj_set_style_bg_color(btn_n, lv_color_hex(0x334155), 0);
    lv_obj_add_event_cb(btn_n, on_next_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ln = lv_label_create(btn_n);
    lv_label_set_text(ln, LV_SYMBOL_NEXT);
    lv_obj_center(ln);

    // 中间微型页码
    s_fs_page_label = lv_label_create(s_fs_bottom_bar);
    lv_label_set_text(s_fs_page_label, "1/9");
    lv_obj_set_style_text_color(s_fs_page_label, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(s_fs_page_label, font_cn, 0);
    lv_obj_align(s_fs_page_label, LV_ALIGN_BOTTOM_MID, 12, -4);

    apply_current_theme();
    load_and_refresh_page(s_current_page);
}

static void on_btn_fullscreen_event(lv_event_t *e)
{
    ui_novel_reader_open_fullscreen();
}

void ui_novel_reader_init(lv_obj_t *parent_tab)
{
    if (!parent_tab) return;

    const lv_font_t *font_cn = sys_font_manager_get_font(14);

    // 1. 顶部标题与图书章节信息
    lv_obj_t *title = lv_label_create(parent_tab);
    lv_label_set_text(title, "《三体 · 连载》");
    lv_obj_set_style_text_color(title, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_text_font(title, font_cn, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 2);

    s_tab_page_label = lv_label_create(parent_tab);
    lv_label_set_text(s_tab_page_label, "第 1 / 9 页");
    lv_obj_set_style_text_color(s_tab_page_label, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(s_tab_page_label, font_cn, 0);
    lv_obj_align(s_tab_page_label, LV_ALIGN_TOP_RIGHT, -4, 2);

    // 2. 小说阅读主卡片 (包含正文展示)
    s_tab_card = lv_obj_create(parent_tab);
    lv_obj_set_size(s_tab_card, 228, 134);
    lv_obj_align(s_tab_card, LV_ALIGN_TOP_MID, 0, 22);
    lv_obj_set_style_bg_color(s_tab_card, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(s_tab_card, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(s_tab_card, 1, 0);
    lv_obj_set_style_radius(s_tab_card, 8, 0);
    lv_obj_set_style_pad_all(s_tab_card, 6, 0);
    lv_obj_clear_flag(s_tab_card, LV_OBJ_FLAG_SCROLLABLE);

    s_tab_text_label = lv_label_create(s_tab_card);
    lv_obj_set_width(s_tab_text_label, 214);
    lv_label_set_long_mode(s_tab_text_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(s_tab_text_label, 3, 0);
    lv_obj_set_style_text_color(s_tab_text_label, lv_color_hex(0xF1F5F9), 0);
    lv_obj_set_style_text_font(s_tab_text_label, font_cn, 0);
    lv_obj_align(s_tab_text_label, LV_ALIGN_TOP_LEFT, 0, 0);

    // 3. 快捷操作条 (上一页、全屏阅读、下一页)
    lv_obj_t *btn_prev = lv_button_create(parent_tab);
    lv_obj_set_size(btn_prev, 46, 32);
    lv_obj_align(btn_prev, LV_ALIGN_TOP_LEFT, 0, 160);
    lv_obj_set_style_bg_color(btn_prev, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(btn_prev, 6, 0);
    lv_obj_clear_flag(btn_prev, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_prev, on_prev_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lp = lv_label_create(btn_prev);
    lv_label_set_text(lp, LV_SYMBOL_PREV);
    lv_obj_center(lp);

    lv_obj_t *btn_fullscreen = lv_button_create(parent_tab);
    lv_obj_set_size(btn_fullscreen, 128, 32);
    lv_obj_align(btn_fullscreen, LV_ALIGN_TOP_MID, 0, 160);
    lv_obj_set_style_bg_color(btn_fullscreen, lv_color_hex(0x2563EB), 0);
    lv_obj_set_style_radius(btn_fullscreen, 6, 0);
    lv_obj_clear_flag(btn_fullscreen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_fullscreen, on_btn_fullscreen_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_fs = lv_label_create(btn_fullscreen);
    lv_label_set_text(lbl_fs, LV_SYMBOL_PLAY " 沉浸全屏阅读");
    lv_obj_set_style_text_font(lbl_fs, font_cn, 0);
    lv_obj_center(lbl_fs);

    lv_obj_t *btn_next = lv_button_create(parent_tab);
    lv_obj_set_size(btn_next, 46, 32);
    lv_obj_align(btn_next, LV_ALIGN_TOP_RIGHT, 0, 160);
    lv_obj_set_style_bg_color(btn_next, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(btn_next, 6, 0);
    lv_obj_clear_flag(btn_next, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_next, on_next_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ln = lv_label_create(btn_next);
    lv_label_set_text(ln, LV_SYMBOL_NEXT);
    lv_obj_center(ln);

    // 4. 辅助工具条 (存书签、读标签、章节跳转)
    lv_obj_t *btn_bm_save = lv_button_create(parent_tab);
    lv_obj_set_size(btn_bm_save, 72, 28);
    lv_obj_align(btn_bm_save, LV_ALIGN_TOP_LEFT, 0, 198);
    lv_obj_set_style_bg_color(btn_bm_save, lv_color_hex(0xF59E0B), 0);
    lv_obj_set_style_radius(btn_bm_save, 6, 0);
    lv_obj_clear_flag(btn_bm_save, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_bm_save, on_bookmark_save_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l_bms = lv_label_create(btn_bm_save);
    lv_label_set_text(l_bms, LV_SYMBOL_SAVE " 存书签");
    lv_obj_set_style_text_font(l_bms, font_cn, 0);
    lv_obj_center(l_bms);

    lv_obj_t *btn_bm_load = lv_button_create(parent_tab);
    lv_obj_set_size(btn_bm_load, 72, 28);
    lv_obj_align(btn_bm_load, LV_ALIGN_TOP_MID, 0, 198);
    lv_obj_set_style_bg_color(btn_bm_load, lv_color_hex(0x10B981), 0);
    lv_obj_set_style_radius(btn_bm_load, 6, 0);
    lv_obj_clear_flag(btn_bm_load, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_bm_load, on_bookmark_load_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l_bml = lv_label_create(btn_bm_load);
    lv_label_set_text(l_bml, LV_SYMBOL_FILE " 读书签");
    lv_obj_set_style_text_font(l_bml, font_cn, 0);
    lv_obj_center(l_bml);

    lv_obj_t *btn_jump = lv_button_create(parent_tab);
    lv_obj_set_size(btn_jump, 72, 28);
    lv_obj_align(btn_jump, LV_ALIGN_TOP_RIGHT, 0, 198);
    lv_obj_set_style_bg_color(btn_jump, lv_color_hex(0x06B6D4), 0);
    lv_obj_set_style_radius(btn_jump, 6, 0);
    lv_obj_clear_flag(btn_jump, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_jump, on_jump_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lj = lv_label_create(btn_jump);
    lv_label_set_text(lj, LV_SYMBOL_RIGHT " 章节");
    lv_obj_set_style_text_font(lj, font_cn, 0);
    lv_obj_center(lj);

    apply_current_theme();
    load_and_refresh_page(0);
}

void ui_novel_reader_refresh_content(void)
{
    apply_current_theme();
    load_and_refresh_page(s_current_page);
}
