/**
 * @file ui_pomodoro.c
 * @brief 专注番茄钟 + 大号数字时钟
 *
 * 双模式切换：
 *  - 【大时钟模式】：当前时间与真实日期
 *  - 【番茄钟模式】：专注 25 分钟 / 休息 5 分钟阶段倒计时
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
static lv_obj_t *s_lbl_clock_time = NULL;  // HH:MM:SS
static lv_obj_t *s_lbl_clock_date = NULL;  // 星期 日期

// ── 番茄钟模式 ──────────────────────────────────────────────────────────────
static lv_obj_t *s_pomo_panel       = NULL;
static lv_obj_t *s_lbl_pomo_title   = NULL;
static lv_obj_t *s_lbl_pomo_phase   = NULL;
static lv_obj_t *s_lbl_pomo_time    = NULL;  // 单一 MM:SS 倒计时标签
static lv_obj_t *s_lbl_pomo_state   = NULL;
static lv_obj_t *s_bar_pomo_progress = NULL;
static lv_obj_t *s_btn_start        = NULL;
static lv_obj_t *s_lbl_btn_start  = NULL;
static lv_obj_t *s_btn_skip       = NULL;
static lv_obj_t *s_btn_reset      = NULL;
static lv_obj_t *s_btn_mode       = NULL;
static lv_obj_t *s_lbl_btn_mode   = NULL;

// ── 状态变量 ────────────────────────────────────────────────────────────────
typedef enum {
    POMO_PHASE_FOCUS = 0,
    POMO_PHASE_REST,
} pomo_phase_t;

#define POMO_FOCUS_SECONDS (25 * 60)
#define POMO_REST_SECONDS  (5 * 60)

static bool s_mode_clock = true;
static bool s_pomo_running = false;
static pomo_phase_t s_pomo_phase = POMO_PHASE_FOCUS;
static int s_pomo_total_sec = POMO_FOCUS_SECONDS;
static int s_pomo_remain_sec = POMO_FOCUS_SECONDS;

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

static int pomo_phase_duration(pomo_phase_t phase)
{
    return phase == POMO_PHASE_FOCUS ? POMO_FOCUS_SECONDS : POMO_REST_SECONDS;
}

static const char *pomo_phase_title(pomo_phase_t phase)
{
    return phase == POMO_PHASE_FOCUS ? "专注 · 25分钟" : "休息 · 5分钟";
}

static void update_pomo_controls(void)
{
    const lv_font_t *font_cn = sys_font_manager_get_font(14);
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
        const char *state = "已暂停";
        if (s_pomo_running) {
            state = s_pomo_phase == POMO_PHASE_FOCUS ? "专注进行中" : "休息进行中";
        } else if (s_pomo_remain_sec == s_pomo_total_sec) {
            state = s_pomo_phase == POMO_PHASE_FOCUS ? "准备开始专注" : "准备开始休息";
        }
        lv_label_set_text(s_lbl_pomo_state, state);
        lv_obj_set_style_text_font(s_lbl_pomo_state, font_cn, 0);
    }
}

/* ── 更新番茄钟 UI：始终用单个固定格式的 MM:SS 标签 ───────────────────────── */
static void update_pomo_ui(void)
{
    if (!s_pomo_panel || !s_lbl_pomo_time || !s_bar_pomo_progress) return;

    int remain_m = s_pomo_remain_sec / 60;
    int remain_s = s_pomo_remain_sec % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", remain_m, remain_s);
    lv_label_set_text(s_lbl_pomo_time, buf);
    lv_bar_set_value(s_bar_pomo_progress,
                     ((s_pomo_total_sec - s_pomo_remain_sec) * 100) / s_pomo_total_sec,
                     LV_ANIM_OFF);

    if (s_lbl_pomo_phase) {
        lv_label_set_text(s_lbl_pomo_phase, pomo_phase_title(s_pomo_phase));
        lv_obj_set_style_text_color(s_lbl_pomo_phase,
            s_pomo_phase == POMO_PHASE_FOCUS ? lv_color_hex(0xFB7185) : lv_color_hex(0x38BDF8), 0);
    }
    update_pomo_controls();
}

static void set_pomo_phase(pomo_phase_t phase)
{
    s_pomo_phase = phase;
    s_pomo_total_sec = pomo_phase_duration(phase);
    s_pomo_remain_sec = s_pomo_total_sec;
    s_pomo_running = false;
    update_pomo_ui();
}

static void advance_pomo_phase(void)
{
    set_pomo_phase(s_pomo_phase == POMO_PHASE_FOCUS ? POMO_PHASE_REST : POMO_PHASE_FOCUS);
}

/* ── 按钮回调 ─────────────────────────────────────────────────────────────── */
static void on_start_clicked(lv_event_t *e)
{
    s_pomo_running = !s_pomo_running;
    update_pomo_controls();
}

static void on_skip_clicked(lv_event_t *e)
{
    advance_pomo_phase();
}

static void on_reset_clicked(lv_event_t *e)
{
    s_pomo_running = false;
    s_pomo_remain_sec = s_pomo_total_sec;
    update_pomo_ui();
}

static void on_mode_clicked(lv_event_t *e)
{
    const lv_font_t *font_cn = sys_font_manager_get_font(14);
    s_mode_clock = !s_mode_clock;
    if (s_mode_clock) {
        lv_obj_clear_flag(s_clock_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_pomo_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_btn_start, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_btn_skip, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_btn_reset, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_lbl_btn_mode, "切换到番茄钟");
    } else {
        lv_obj_add_flag(s_clock_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_pomo_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_btn_start, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_btn_skip, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_btn_reset, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_lbl_btn_mode, "切换到大时钟");
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
    lv_obj_set_size(s_clock_panel, 228, 220);
    lv_obj_set_pos(s_clock_panel, 6, 4);
    lv_obj_set_style_bg_color(s_clock_panel, lv_color_hex(0x030712), 0);
    lv_obj_set_style_border_color(s_clock_panel, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_border_width(s_clock_panel, 1, 0);
    lv_obj_set_style_radius(s_clock_panel, 12, 0);
    lv_obj_set_style_pad_all(s_clock_panel, 4, 0);
    lv_obj_clear_flag(s_clock_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_clock_title = lv_label_create(s_clock_panel);
    lv_label_set_text(lbl_clock_title, "大时钟");
    lv_obj_set_style_text_font(lbl_clock_title, font_cn, 0);
    lv_obj_set_style_text_color(lbl_clock_title, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(lbl_clock_title, LV_ALIGN_TOP_LEFT, 10, 8);

    // 使用单个标签显示完整 HH:MM:SS，避免分钟和秒数错位
    s_lbl_clock_time = lv_label_create(s_clock_panel);
    lv_obj_set_width(s_lbl_clock_time, 210);
    lv_label_set_text(s_lbl_clock_time, "--:--:--");
    lv_obj_set_style_text_font(s_lbl_clock_time, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(s_lbl_clock_time, lv_color_hex(0xE2E8F0), 0);
    lv_obj_set_style_text_align(s_lbl_clock_time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_lbl_clock_time, LV_ALIGN_TOP_MID, 0, 52);

    // 星期 + 日期：使用高对比度颜色，并由真实网络时间更新
    s_lbl_clock_date = lv_label_create(s_clock_panel);
    lv_obj_set_width(s_lbl_clock_date, 210);
    lv_label_set_text(s_lbl_clock_date, "正在同步日期...");
    lv_obj_set_style_text_font(s_lbl_clock_date, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_clock_date, lv_color_hex(0xCBD5E1), 0);
    lv_obj_set_style_text_align(s_lbl_clock_date, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_lbl_clock_date, LV_ALIGN_TOP_MID, 0, 116);

    /* ====================================================================
     * 番茄钟面板（默认隐藏）
     * ==================================================================== */
    s_pomo_panel = lv_obj_create(parent_tab);
    lv_obj_set_size(s_pomo_panel, 228, 168);
    lv_obj_set_pos(s_pomo_panel, 6, 4);
    lv_obj_set_style_bg_color(s_pomo_panel, lv_color_hex(0x030712), 0);
    lv_obj_set_style_border_color(s_pomo_panel, lv_color_hex(0x7F1D1D), 0);
    lv_obj_set_style_border_width(s_pomo_panel, 1, 0);
    lv_obj_set_style_radius(s_pomo_panel, 12, 0);
    lv_obj_set_style_pad_all(s_pomo_panel, 0, 0);
    lv_obj_clear_flag(s_pomo_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_pomo_panel, LV_OBJ_FLAG_HIDDEN);

    s_lbl_pomo_title = lv_label_create(s_pomo_panel);
    lv_label_set_text(s_lbl_pomo_title, "番茄时钟");
    lv_obj_set_style_text_font(s_lbl_pomo_title, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_pomo_title, lv_color_hex(0xFB7185), 0);
    lv_obj_align(s_lbl_pomo_title, LV_ALIGN_TOP_LEFT, 10, 8);

    s_lbl_pomo_phase = lv_label_create(s_pomo_panel);
    lv_label_set_text(s_lbl_pomo_phase, "专注 · 25分钟");
    lv_obj_set_style_text_font(s_lbl_pomo_phase, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_pomo_phase, lv_color_hex(0xFB7185), 0);
    lv_obj_align(s_lbl_pomo_phase, LV_ALIGN_TOP_RIGHT, -10, 8);

    // 单个固定宽度标签显示剩余时间，分钟和秒永远不会相互错位
    s_lbl_pomo_time = lv_label_create(s_pomo_panel);
    lv_obj_set_width(s_lbl_pomo_time, 210);
    lv_label_set_text(s_lbl_pomo_time, "25:00");
    lv_obj_set_style_text_font(s_lbl_pomo_time, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(s_lbl_pomo_time, lv_color_hex(0xFEF2F2), 0);
    lv_obj_set_style_text_align(s_lbl_pomo_time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_lbl_pomo_time, LV_ALIGN_TOP_MID, 0, 42);

    // 状态描述
    s_lbl_pomo_state = lv_label_create(s_pomo_panel);
    lv_label_set_text(s_lbl_pomo_state, "准备开始专注");
    lv_obj_set_style_text_font(s_lbl_pomo_state, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_pomo_state, lv_color_hex(0xCBD5E1), 0);
    lv_obj_align(s_lbl_pomo_state, LV_ALIGN_TOP_MID, 0, 88);

    s_bar_pomo_progress = lv_bar_create(s_pomo_panel);
    lv_obj_set_size(s_bar_pomo_progress, 204, 8);
    lv_obj_align(s_bar_pomo_progress, LV_ALIGN_TOP_MID, 0, 122);
    lv_bar_set_range(s_bar_pomo_progress, 0, 100);
    lv_bar_set_value(s_bar_pomo_progress, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar_pomo_progress, lv_color_hex(0x1E293B), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar_pomo_progress, lv_color_hex(0xFB7185), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_bar_pomo_progress, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(s_bar_pomo_progress, 4, LV_PART_INDICATOR);

    /* ── 操作按钮行：开始/暂停、跳过、重置 ─────────────────────────────────── */
    s_btn_start = lv_button_create(parent_tab);
    lv_obj_set_size(s_btn_start, 72, 38);
    lv_obj_set_pos(s_btn_start, 6, 180);
    lv_obj_set_style_bg_color(s_btn_start, lv_color_hex(0x14532D), 0);
    lv_obj_set_style_border_color(s_btn_start, lv_color_hex(0x22C55E), 0);
    lv_obj_set_style_border_width(s_btn_start, 1, 0);
    lv_obj_set_style_radius(s_btn_start, 8, 0);
    lv_obj_set_style_pad_all(s_btn_start, 0, 0);
    lv_obj_add_flag(s_btn_start, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_btn_start, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_btn_start, on_start_clicked, LV_EVENT_CLICKED, NULL);
    s_lbl_btn_start = lv_label_create(s_btn_start);
    lv_label_set_text(s_lbl_btn_start, "开始");
    lv_obj_set_style_text_font(s_lbl_btn_start, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_btn_start, lv_color_hex(0x86EFAC), 0);
    lv_obj_center(s_lbl_btn_start);

    s_btn_skip = lv_button_create(parent_tab);
    lv_obj_set_size(s_btn_skip, 72, 38);
    lv_obj_set_pos(s_btn_skip, 82, 180);
    lv_obj_set_style_bg_color(s_btn_skip, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_border_color(s_btn_skip, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_border_width(s_btn_skip, 1, 0);
    lv_obj_set_style_radius(s_btn_skip, 8, 0);
    lv_obj_add_flag(s_btn_skip, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_btn_skip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_btn_skip, on_skip_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_skip = lv_label_create(s_btn_skip);
    lv_label_set_text(lbl_skip, "跳过");
    lv_obj_set_style_text_font(lbl_skip, font_cn, 0);
    lv_obj_center(lbl_skip);

    s_btn_reset = lv_button_create(parent_tab);
    lv_obj_set_size(s_btn_reset, 72, 38);
    lv_obj_set_pos(s_btn_reset, 158, 180);
    lv_obj_set_style_bg_color(s_btn_reset, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(s_btn_reset, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(s_btn_reset, 1, 0);
    lv_obj_set_style_radius(s_btn_reset, 8, 0);
    lv_obj_set_style_pad_all(s_btn_reset, 0, 0);
    lv_obj_add_flag(s_btn_reset, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_btn_reset, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_btn_reset, on_reset_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_reset = lv_label_create(s_btn_reset);
    lv_label_set_text(lbl_reset, "重置");
    lv_obj_set_style_text_font(lbl_reset, font_cn, 0);
    lv_obj_set_style_text_color(lbl_reset, lv_color_hex(0x94A3B8), 0);
    lv_obj_center(lbl_reset);

    /* ── 模式切换按钮（位于底部，始终可见） ───────────────────────────────── */
    s_btn_mode = lv_button_create(parent_tab);
    lv_obj_set_size(s_btn_mode, 228, 30);
    lv_obj_set_pos(s_btn_mode, 6, 230);
    lv_obj_set_style_bg_color(s_btn_mode, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(s_btn_mode, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_border_width(s_btn_mode, 1, 0);
    lv_obj_set_style_radius(s_btn_mode, 8, 0);
    lv_obj_set_style_pad_all(s_btn_mode, 0, 0);
    lv_obj_clear_flag(s_btn_mode, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_btn_mode, on_mode_clicked, LV_EVENT_CLICKED, NULL);
    s_lbl_btn_mode = lv_label_create(s_btn_mode);
    lv_label_set_text(s_lbl_btn_mode, "切换到番茄钟");
    lv_obj_set_style_text_font(s_lbl_btn_mode, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_btn_mode, lv_color_hex(0x38BDF8), 0);
    lv_obj_center(s_lbl_btn_mode);

    update_pomo_ui();
    ESP_LOGI(TAG, "番茄钟/大时钟 UI 初始化完毕：专注25分钟，休息5分钟");
}

/* ── 公共接口：每秒更新（由主 tick 调用） ────────────────────────────────── */
void ui_pomodoro_tick(int hour, int min, int sec)
{
    if (!s_lbl_clock_time) return;

    bsp_lvgl_port_lock(0);

    /* -- 大时钟刷新 -- */
    if (s_mode_clock) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, min, sec);
        lv_label_set_text(s_lbl_clock_time, buf);
    }

    /* -- 番茄钟计时 -- */
    if (!s_mode_clock && s_pomo_running) {
        if (s_pomo_remain_sec > 0) {
            s_pomo_remain_sec--;
            update_pomo_ui();
        }
        if (s_pomo_remain_sec == 0) {
            // 当前阶段完成后停在下一阶段，让用户明确看到下一步再开始。
            advance_pomo_phase();
            xTaskCreate(led_blink_task, "led_blink", 1024, NULL, 3, NULL);
            ESP_LOGI(TAG, "番茄钟阶段完成，切换到%s", s_pomo_phase == POMO_PHASE_FOCUS ? "专注" : "休息");
        }
    }

    bsp_lvgl_port_unlock();
}

void ui_pomodoro_update_date(const char *date_str)
{
    if (!date_str || !s_lbl_clock_date) return;

    bsp_lvgl_port_lock(0);
    lv_label_set_text(s_lbl_clock_date, date_str);
    bsp_lvgl_port_unlock();
}
