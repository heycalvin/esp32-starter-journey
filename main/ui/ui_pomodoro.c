/**
 * @file ui_pomodoro.c
 * @brief 专注番茄钟 + 大号数字时钟
 *
 * 双模式切换：
 *  - 【大时钟模式】：HH:MM:SS 超大字体居中显示
 *  - 【番茄钟模式】：25 分钟倒计时，圆弧进度环，完成时 LED 闪烁
 *
 * 设计风格：午夜深蓝 + 荧光红番茄弧 + 极客扁平卡片。
 */
#include "ui_pomodoro.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "lvgl.h"
#include "bsp_lvgl_port.h"
#include "bsp_led.h"
#include "sys_font_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "POMODORO";

// ── 大时钟模式 ──────────────────────────────────────────────────────────────
static lv_obj_t *s_clock_panel    = NULL;
static lv_obj_t *s_lbl_clock_big  = NULL;  // HH:MM
static lv_obj_t *s_lbl_clock_sec  = NULL;  // :SS
static lv_obj_t *s_lbl_clock_date = NULL;  // 星期 日期

// ── 番茄钟模式 ──────────────────────────────────────────────────────────────
static lv_obj_t *s_pomo_panel     = NULL;
static lv_obj_t *s_arc_pomo       = NULL;  // 圆弧进度环
static lv_obj_t *s_lbl_pomo_time  = NULL;  // 剩余时间 MM:SS
static lv_obj_t *s_lbl_pomo_state = NULL;  // 状态描述
static lv_obj_t *s_btn_start      = NULL;  // 开始/暂停
static lv_obj_t *s_lbl_btn_start  = NULL;
static lv_obj_t *s_btn_reset      = NULL;  // 重置
static lv_obj_t *s_btn_mode       = NULL;  // 切换模式（所有模式共用）
static lv_obj_t *s_lbl_btn_mode   = NULL;

// ── 状态变量 ────────────────────────────────────────────────────────────────
static bool  s_mode_clock   = true;   // true = 大时钟，false = 番茄钟
static bool  s_pomo_running = false;
static int   s_pomo_total_sec = 25 * 60; // 默认 25 分钟
static int   s_pomo_remain_sec = 25 * 60;

/* ── LED 闪烁任务（番茄钟完成提醒） ──────────────────────────────────────── */
static void led_blink_task(void *arg)
{
    for (int i = 0; i < 5; i++) {
        bsp_led_set(true);
        vTaskDelay(pdMS_TO_TICKS(200));
        bsp_led_set(false);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    vTaskDelete(NULL);
}

/* ── 更新番茄钟 UI ─────────────────────────────────────────────────────────── */
static void update_pomo_ui(void)
{
    if (!s_pomo_panel || !s_arc_pomo) return;

    int remain_m = s_pomo_remain_sec / 60;
    int remain_s = s_pomo_remain_sec % 60;

    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", remain_m, remain_s);
    lv_label_set_text(s_lbl_pomo_time, buf);

    // 圆弧进度（从 360 到 0，顺时针消耗）
    int32_t arc_val = (int32_t)((s_pomo_remain_sec * 360) / s_pomo_total_sec);
    lv_arc_set_value(s_arc_pomo, arc_val);

    // 颜色随时间变化：剩余 < 5min 变橙色，< 1min 变红色
    uint32_t arc_color = 0xEF4444;
    if (s_pomo_remain_sec > 5 * 60) arc_color = 0xEF4444;
    else if (s_pomo_remain_sec > 60) arc_color = 0xF97316;
    else arc_color = 0xDC2626;
    lv_obj_set_style_arc_color(s_arc_pomo, lv_color_hex(arc_color), LV_PART_INDICATOR);
}

/* ── 按钮回调 ─────────────────────────────────────────────────────────────── */
static void on_start_clicked(lv_event_t *e)
{
    const lv_font_t *font_cn = sys_font_manager_get_font(14);
    s_pomo_running = !s_pomo_running;
    if (s_lbl_btn_start) {
        lv_label_set_text(s_lbl_btn_start, s_pomo_running ? "暂停" : "开始");
        lv_obj_set_style_text_font(s_lbl_btn_start, font_cn, 0);
    }
    if (s_btn_start) {
        lv_obj_set_style_bg_color(s_btn_start,
            s_pomo_running ? lv_color_hex(0x7F1D1D) : lv_color_hex(0x14532D), 0);
        lv_obj_set_style_border_color(s_btn_start,
            s_pomo_running ? lv_color_hex(0xEF4444) : lv_color_hex(0x22C55E), 0);
    }
    if (s_lbl_pomo_state) {
        lv_label_set_text(s_lbl_pomo_state, s_pomo_running ? "专注中..." : "已暂停");
        lv_obj_set_style_text_font(s_lbl_pomo_state, font_cn, 0);
    }
}

static void on_reset_clicked(lv_event_t *e)
{
    const lv_font_t *font_cn = sys_font_manager_get_font(14);
    s_pomo_running = false;
    s_pomo_remain_sec = s_pomo_total_sec;
    update_pomo_ui();
    if (s_lbl_btn_start) {
        lv_label_set_text(s_lbl_btn_start, "开始");
        lv_obj_set_style_text_font(s_lbl_btn_start, font_cn, 0);
    }
    if (s_btn_start) {
        lv_obj_set_style_bg_color(s_btn_start, lv_color_hex(0x14532D), 0);
        lv_obj_set_style_border_color(s_btn_start, lv_color_hex(0x22C55E), 0);
    }
    if (s_lbl_pomo_state) {
        lv_label_set_text(s_lbl_pomo_state, "准备开始 25 分钟专注");
        lv_obj_set_style_text_font(s_lbl_pomo_state, font_cn, 0);
    }
}

static void on_mode_clicked(lv_event_t *e)
{
    const lv_font_t *font_cn = sys_font_manager_get_font(14);
    s_mode_clock = !s_mode_clock;
    if (s_mode_clock) {
        lv_obj_clear_flag(s_clock_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_pomo_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_btn_start, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_btn_reset, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_lbl_btn_mode, "番茄钟");
    } else {
        lv_obj_add_flag(s_clock_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_pomo_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_btn_start, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_btn_reset, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_lbl_btn_mode, "大时钟");
    }
    lv_obj_set_style_text_font(s_lbl_btn_mode, font_cn, 0);
}

/* ── 公共接口：初始化 UI ──────────────────────────────────────────────────── */
void ui_pomodoro_init(lv_obj_t *parent_tab)
{
    if (!parent_tab) return;
    const lv_font_t *font_cn = sys_font_manager_get_font(14);

    /* ====================================================================
     * 大时钟面板（默认显示）
     * ==================================================================== */
    s_clock_panel = lv_obj_create(parent_tab);
    lv_obj_set_size(s_clock_panel, 228, 180);
    lv_obj_set_pos(s_clock_panel, 6, 4);
    lv_obj_set_style_bg_color(s_clock_panel, lv_color_hex(0x030712), 0);
    lv_obj_set_style_border_color(s_clock_panel, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_border_width(s_clock_panel, 1, 0);
    lv_obj_set_style_radius(s_clock_panel, 12, 0);
    lv_obj_set_style_pad_all(s_clock_panel, 4, 0);
    lv_obj_clear_flag(s_clock_panel, LV_OBJ_FLAG_SCROLLABLE);

    // HH:MM 超大字
    s_lbl_clock_big = lv_label_create(s_clock_panel);
    lv_label_set_text(s_lbl_clock_big, "--:--");
    lv_obj_set_style_text_font(s_lbl_clock_big, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_lbl_clock_big, lv_color_hex(0xE2E8F0), 0);
    lv_obj_align(s_lbl_clock_big, LV_ALIGN_CENTER, 0, -20);

    // :SS 秒数（小一些）
    s_lbl_clock_sec = lv_label_create(s_clock_panel);
    lv_label_set_text(s_lbl_clock_sec, ":--");
    lv_obj_set_style_text_font(s_lbl_clock_sec, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_lbl_clock_sec, lv_color_hex(0x64748B), 0);
    lv_obj_align(s_lbl_clock_sec, LV_ALIGN_CENTER, 68, 5);

    // 星期 + 日期
    s_lbl_clock_date = lv_label_create(s_clock_panel);
    lv_label_set_text(s_lbl_clock_date, "-- 年 -- 月 -- 日");
    lv_obj_set_style_text_font(s_lbl_clock_date, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_clock_date, lv_color_hex(0x334155), 0);
    lv_obj_align(s_lbl_clock_date, LV_ALIGN_BOTTOM_MID, 0, -4);

    /* ====================================================================
     * 番茄钟面板（默认隐藏）
     * ==================================================================== */
    s_pomo_panel = lv_obj_create(parent_tab);
    lv_obj_set_size(s_pomo_panel, 228, 180);
    lv_obj_set_pos(s_pomo_panel, 6, 4);
    lv_obj_set_style_bg_color(s_pomo_panel, lv_color_hex(0x030712), 0);
    lv_obj_set_style_border_color(s_pomo_panel, lv_color_hex(0x7F1D1D), 0);
    lv_obj_set_style_border_width(s_pomo_panel, 1, 0);
    lv_obj_set_style_radius(s_pomo_panel, 12, 0);
    lv_obj_set_style_pad_all(s_pomo_panel, 0, 0);
    lv_obj_clear_flag(s_pomo_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_pomo_panel, LV_OBJ_FLAG_HIDDEN);

    // 圆弧进度环
    s_arc_pomo = lv_arc_create(s_pomo_panel);
    lv_obj_set_size(s_arc_pomo, 140, 140);
    lv_obj_center(s_arc_pomo);
    lv_arc_set_rotation(s_arc_pomo, 270);
    lv_arc_set_bg_angles(s_arc_pomo, 0, 360);
    lv_arc_set_range(s_arc_pomo, 0, 360);
    lv_arc_set_value(s_arc_pomo, 360);
    lv_obj_remove_style(s_arc_pomo, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_arc_pomo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(s_arc_pomo, lv_color_hex(0x1E293B), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc_pomo, lv_color_hex(0xEF4444), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_arc_pomo, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc_pomo, 10, LV_PART_INDICATOR);

    // 剩余时间大字
    s_lbl_pomo_time = lv_label_create(s_pomo_panel);
    lv_label_set_text(s_lbl_pomo_time, "25:00");
    lv_obj_set_style_text_font(s_lbl_pomo_time, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(s_lbl_pomo_time, lv_color_hex(0xFEF2F2), 0);
    lv_obj_align(s_lbl_pomo_time, LV_ALIGN_CENTER, 0, -6);

    // 状态描述
    s_lbl_pomo_state = lv_label_create(s_pomo_panel);
    lv_label_set_text(s_lbl_pomo_state, "准备开始 25 分钟专注");
    lv_obj_set_style_text_font(s_lbl_pomo_state, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_pomo_state, lv_color_hex(0x64748B), 0);
    lv_obj_align(s_lbl_pomo_state, LV_ALIGN_CENTER, 0, 30);

    /* ── 操作按钮行（开始 + 重置，Y: 188） ───────────────────────────────── */
    s_btn_start = lv_obj_create(parent_tab);
    lv_obj_set_size(s_btn_start, 106, 34);
    lv_obj_set_pos(s_btn_start, 6, 188);
    lv_obj_set_style_bg_color(s_btn_start, lv_color_hex(0x14532D), 0);
    lv_obj_set_style_border_color(s_btn_start, lv_color_hex(0x22C55E), 0);
    lv_obj_set_style_border_width(s_btn_start, 1, 0);
    lv_obj_set_style_radius(s_btn_start, 8, 0);
    lv_obj_set_style_pad_all(s_btn_start, 0, 0);
    lv_obj_add_flag(s_btn_start, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_btn_start, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_btn_start, on_start_clicked, LV_EVENT_CLICKED, NULL);
    s_lbl_btn_start = lv_label_create(s_btn_start);
    lv_label_set_text(s_lbl_btn_start, "开始");
    lv_obj_set_style_text_font(s_lbl_btn_start, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_btn_start, lv_color_hex(0x86EFAC), 0);
    lv_obj_center(s_lbl_btn_start);

    s_btn_reset = lv_obj_create(parent_tab);
    lv_obj_set_size(s_btn_reset, 106, 34);
    lv_obj_set_pos(s_btn_reset, 116, 188);
    lv_obj_set_style_bg_color(s_btn_reset, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(s_btn_reset, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(s_btn_reset, 1, 0);
    lv_obj_set_style_radius(s_btn_reset, 8, 0);
    lv_obj_set_style_pad_all(s_btn_reset, 0, 0);
    lv_obj_add_flag(s_btn_reset, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_btn_reset, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_btn_reset, on_reset_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_reset = lv_label_create(s_btn_reset);
    lv_label_set_text(lbl_reset, "重置");
    lv_obj_set_style_text_font(lbl_reset, font_cn, 0);
    lv_obj_set_style_text_color(lbl_reset, lv_color_hex(0x94A3B8), 0);
    lv_obj_center(lbl_reset);

    /* ── 模式切换按钮（位于底部，始终可见） ───────────────────────────────── */
    s_btn_mode = lv_obj_create(parent_tab);
    lv_obj_set_size(s_btn_mode, 228, 30);
    lv_obj_set_pos(s_btn_mode, 6, 228);
    lv_obj_set_style_bg_color(s_btn_mode, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(s_btn_mode, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_border_width(s_btn_mode, 1, 0);
    lv_obj_set_style_radius(s_btn_mode, 8, 0);
    lv_obj_set_style_pad_all(s_btn_mode, 0, 0);
    lv_obj_add_flag(s_btn_mode, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_btn_mode, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_btn_mode, on_mode_clicked, LV_EVENT_CLICKED, NULL);
    s_lbl_btn_mode = lv_label_create(s_btn_mode);
    lv_label_set_text(s_lbl_btn_mode, "番茄钟");
    lv_obj_set_style_text_font(s_lbl_btn_mode, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_btn_mode, lv_color_hex(0x38BDF8), 0);
    lv_obj_center(s_lbl_btn_mode);

    ESP_LOGI(TAG, "番茄钟/大时钟 UI 初始化完毕");
}

/* ── 公共接口：每秒更新（由主 tick 调用） ────────────────────────────────── */
void ui_pomodoro_tick(int hour, int min, int sec)
{
    if (!s_lbl_clock_big) return;

    bsp_lvgl_port_lock(0);

    /* -- 大时钟刷新 -- */
    if (s_mode_clock) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", hour, min);
        lv_label_set_text(s_lbl_clock_big, buf);
        snprintf(buf, sizeof(buf), ":%02d", sec);
        lv_label_set_text(s_lbl_clock_sec, buf);
    }

    /* -- 番茄钟计时 -- */
    if (!s_mode_clock && s_pomo_running) {
        if (s_pomo_remain_sec > 0) {
            s_pomo_remain_sec--;
            update_pomo_ui();
        }
        if (s_pomo_remain_sec == 0) {
            // 计时完成
            s_pomo_running = false;
            const lv_font_t *font_cn = sys_font_manager_get_font(14);
            if (s_lbl_pomo_state) {
                lv_label_set_text(s_lbl_pomo_state, "休息一下！棒极了！");
                lv_obj_set_style_text_font(s_lbl_pomo_state, font_cn, 0);
                lv_obj_set_style_text_color(s_lbl_pomo_state, lv_color_hex(0xFBBF24), 0);
            }
            if (s_lbl_btn_start) {
                lv_label_set_text(s_lbl_btn_start, "开始");
                lv_obj_set_style_text_font(s_lbl_btn_start, font_cn, 0);
            }
            // LED 闪烁提醒
            xTaskCreate(led_blink_task, "led_blink", 1024, NULL, 3, NULL);
            ESP_LOGI(TAG, "番茄钟完成！");
        }
    }

    bsp_lvgl_port_unlock();
}
