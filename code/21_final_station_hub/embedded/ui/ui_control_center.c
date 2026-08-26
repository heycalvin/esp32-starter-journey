/**
 * @file ui_control_center.c
 * @brief 全局下拉控制中心 (iOS Control Center 风格)
 *
 * 从屏幕顶部向下滑动触发，半透明毛玻璃面板弹性下滑进入。
 * 包含：屏幕亮度滑条、板载 LED 开关、WiFi 状态、系统信息。
 * 上滑或点击遮罩层收起。
 */
#include "ui_control_center.h"
#include <stdbool.h>
#include <stdio.h>
#include "lvgl.h"
#include "bsp_lvgl_port.h"
#include "bsp_display.h"
#include "bsp_led.h"
#include "net_manager.h"
#include "sys_font_manager.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "CC";

// 控制中心面板高度（占屏幕约 65%）
#define CC_PANEL_H   180
#define CC_PANEL_W   240

static lv_obj_t *s_cc_overlay   = NULL;  // 半透明遮罩
static lv_obj_t *s_cc_panel     = NULL;  // 主面板
static lv_obj_t *s_lbl_bright   = NULL;  // 亮度数值标签
static lv_obj_t *s_slider_bl    = NULL;  // 亮度滑条
static lv_obj_t *s_btn_led_cc   = NULL;  // LED 快捷开关
static lv_obj_t *s_lbl_led_cc   = NULL;
static lv_obj_t *s_lbl_wifi_cc  = NULL;  // Wi-Fi 状态
static lv_obj_t *s_lbl_heap_cc  = NULL;  // 内存状态
static bool      s_visible       = false;

/* ── 亮度滑条回调 ─────────────────────────────────────────────── */
static void on_brightness_changed(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    bsp_display_set_backlight_pwm((uint8_t)val);
    if (s_lbl_bright) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", (int)val);
        lv_label_set_text(s_lbl_bright, buf);
    }
}

/* ── LED 快速开关回调 ──────────────────────────────────────────── */
static void on_led_cc_clicked(lv_event_t *e)
{
    bool new_state = !bsp_led_get_state();
    bsp_led_set(new_state);
    if (s_lbl_led_cc) {
        lv_label_set_text(s_lbl_led_cc, new_state ? "照明: 开启" : "照明: 关闭");
    }
    if (s_btn_led_cc) {
        lv_obj_set_style_bg_color(s_btn_led_cc,
            new_state ? lv_color_hex(0x065F46) : lv_color_hex(0x1E293B), 0);
        lv_obj_set_style_border_color(s_btn_led_cc,
            new_state ? lv_color_hex(0x10B981) : lv_color_hex(0x334155), 0);
    }
}

/* ── 收起遮罩点击 ──────────────────────────────────────────────── */
static void on_overlay_clicked(lv_event_t *e)
{
    ui_control_center_hide();
}

/* ── 收起按钮 ──────────────────────────────────────────────────── */
static void on_close_btn_clicked(lv_event_t *e)
{
    ui_control_center_hide();
}

/* ── 动画完成后隐藏遮罩（收起动画完毕时） ─────────────────────── */
static void on_hide_anim_done(lv_anim_t *a)
{
    if (s_cc_overlay) {
        lv_obj_add_flag(s_cc_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ── 展开动画 ──────────────────────────────────────────────────── */
static void anim_y_cb(void *obj, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)obj, v);
}

/* =========================================================================
 * 公共接口
 * ========================================================================= */

void ui_control_center_show(void)
{
    if (s_visible || !s_cc_overlay) return;
    s_visible = true;

    // 刷新 Wi-Fi 与内存状态
    if (s_lbl_wifi_cc) {
        char ip[32] = {0};
        net_manager_get_ip_str(ip, sizeof(ip));
        char buf[64];
        if (net_manager_is_wifi_connected()) {
            snprintf(buf, sizeof(buf), "WiFi: 已连接 · %s", ip);
        } else if (net_manager_is_provisioning()) {
            snprintf(buf, sizeof(buf), "WiFi: AP 配网模式");
        } else {
            snprintf(buf, sizeof(buf), "WiFi: 正在连接...");
        }
        lv_label_set_text(s_lbl_wifi_cc, buf);
    }

    // 同步亮度滑条
    if (s_slider_bl) {
        lv_slider_set_value(s_slider_bl, bsp_display_get_backlight_pct(), LV_ANIM_OFF);
    }
    if (s_lbl_bright) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", bsp_display_get_backlight_pct());
        lv_label_set_text(s_lbl_bright, buf);
    }

    // 同步 LED 状态
    bool led_on = bsp_led_get_state();
    if (s_lbl_led_cc) {
        lv_label_set_text(s_lbl_led_cc, led_on ? "照明: 开启" : "照明: 关闭");
    }
    if (s_btn_led_cc) {
        lv_obj_set_style_bg_color(s_btn_led_cc,
            led_on ? lv_color_hex(0x065F46) : lv_color_hex(0x1E293B), 0);
        lv_obj_set_style_border_color(s_btn_led_cc,
            led_on ? lv_color_hex(0x10B981) : lv_color_hex(0x334155), 0);
    }

    lv_obj_clear_flag(s_cc_overlay, LV_OBJ_FLAG_HIDDEN);

    // 面板从 -CC_PANEL_H 弹性滑入到 0
    lv_obj_set_y(s_cc_panel, -CC_PANEL_H);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, anim_y_cb);
    lv_anim_set_var(&a, s_cc_panel);
    lv_anim_set_values(&a, -CC_PANEL_H, 0);
    lv_anim_set_duration(&a, 260);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_start(&a);
    ESP_LOGI(TAG, "控制中心展开");
}

void ui_control_center_hide(void)
{
    if (!s_visible || !s_cc_panel) return;
    s_visible = false;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, anim_y_cb);
    lv_anim_set_var(&a, s_cc_panel);
    lv_anim_set_values(&a, lv_obj_get_y(s_cc_panel), -CC_PANEL_H);
    lv_anim_set_duration(&a, 220);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_completed_cb(&a, on_hide_anim_done);
    lv_anim_start(&a);
    ESP_LOGI(TAG, "控制中心收起");
}

bool ui_control_center_is_visible(void)
{
    return s_visible;
}

void ui_control_center_init(lv_obj_t *parent)
{
    const lv_font_t *font_cn = sys_font_manager_get_font(14);

    /* ── 半透明遮罩层（全屏，点击收起控制中心） ─────────────────── */
    s_cc_overlay = lv_obj_create(parent);
    lv_obj_set_size(s_cc_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(s_cc_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_cc_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_cc_overlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(s_cc_overlay, 0, 0);
    lv_obj_set_style_radius(s_cc_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_cc_overlay, 0, 0);
    lv_obj_clear_flag(s_cc_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_cc_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_cc_overlay, LV_OBJ_FLAG_HIDDEN);  // 默认隐藏
    lv_obj_add_event_cb(s_cc_overlay, on_overlay_clicked, LV_EVENT_CLICKED, NULL);

    /* ── 主控制面板（毛玻璃深色，从顶部下滑） ───────────────────── */
    s_cc_panel = lv_obj_create(s_cc_overlay);
    lv_obj_set_size(s_cc_panel, CC_PANEL_W, CC_PANEL_H);
    lv_obj_set_pos(s_cc_panel, 0, -CC_PANEL_H);  // 初始在屏幕外
    lv_obj_set_style_bg_color(s_cc_panel, lv_color_hex(0x0B1220), 0);
    lv_obj_set_style_bg_opa(s_cc_panel, LV_OPA_90, 0);
    lv_obj_set_style_border_color(s_cc_panel, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_border_width(s_cc_panel, 1, 0);
    lv_obj_set_style_radius(s_cc_panel, 0, 0);
    lv_obj_set_style_pad_all(s_cc_panel, 0, 0);
    lv_obj_clear_flag(s_cc_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_cc_panel, LV_OBJ_FLAG_CLICKABLE);

    /* ── 顶部标题栏 ─────────────────────────────────────────────── */
    lv_obj_t *top_bar = lv_obj_create(s_cc_panel);
    lv_obj_set_size(top_bar, CC_PANEL_W, 28);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x0A1628), 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    lv_obj_set_style_pad_all(top_bar, 0, 0);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_title = lv_label_create(top_bar);
    lv_label_set_text(lbl_title, "控制中心");
    lv_obj_set_style_text_font(lbl_title, font_cn, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *btn_close = lv_button_create(top_bar);
    lv_obj_set_size(btn_close, 40, 20);
    lv_obj_align(btn_close, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(btn_close, 10, 0);
    lv_obj_add_event_cb(btn_close, on_close_btn_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_x = lv_label_create(btn_close);
    lv_label_set_text(lbl_x, "收起");
    lv_obj_set_style_text_font(lbl_x, font_cn, 0);
    lv_obj_set_style_text_color(lbl_x, lv_color_hex(0xCBD5E1), 0);
    lv_obj_center(lbl_x);

    /* ── 亮度区域 ────────────────────────────────────────────────── */
    lv_obj_t *bright_card = lv_obj_create(s_cc_panel);
    lv_obj_set_size(bright_card, 228, 48);
    lv_obj_set_pos(bright_card, 6, 32);
    lv_obj_set_style_bg_color(bright_card, lv_color_hex(0x0F2040), 0);
    lv_obj_set_style_border_color(bright_card, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_border_width(bright_card, 1, 0);
    lv_obj_set_style_radius(bright_card, 8, 0);
    lv_obj_set_style_pad_all(bright_card, 6, 0);
    lv_obj_clear_flag(bright_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_bl_icon = lv_label_create(bright_card);
    lv_label_set_text(lbl_bl_icon, "亮度");
    lv_obj_set_style_text_font(lbl_bl_icon, font_cn, 0);
    lv_obj_set_style_text_color(lbl_bl_icon, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(lbl_bl_icon, LV_ALIGN_TOP_LEFT, 2, 0);

    s_lbl_bright = lv_label_create(bright_card);
    char bright_buf[16];
    snprintf(bright_buf, sizeof(bright_buf), "%d%%", bsp_display_get_backlight_pct());
    lv_label_set_text(s_lbl_bright, bright_buf);
    lv_obj_set_style_text_font(s_lbl_bright, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_bright, lv_color_hex(0xFBBF24), 0);
    lv_obj_align(s_lbl_bright, LV_ALIGN_TOP_RIGHT, -2, 0);

    s_slider_bl = lv_slider_create(bright_card);
    lv_obj_set_size(s_slider_bl, 214, 12);
    lv_obj_align(s_slider_bl, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_slider_set_range(s_slider_bl, 10, 100);
    lv_slider_set_value(s_slider_bl, bsp_display_get_backlight_pct(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_slider_bl, lv_color_hex(0x1E3A5F), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider_bl, lv_color_hex(0x38BDF8), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider_bl, lv_color_hex(0xF0F9FF), LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_slider_bl, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(s_slider_bl, on_brightness_changed, LV_EVENT_VALUE_CHANGED, NULL);

    /* ── LED 快速开关 ─────────────────────────────────────────────── */
    s_btn_led_cc = lv_obj_create(s_cc_panel);
    lv_obj_set_size(s_btn_led_cc, 108, 48);
    lv_obj_set_pos(s_btn_led_cc, 6, 84);
    bool led_on = bsp_led_get_state();
    lv_obj_set_style_bg_color(s_btn_led_cc,
        led_on ? lv_color_hex(0x065F46) : lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(s_btn_led_cc,
        led_on ? lv_color_hex(0x10B981) : lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(s_btn_led_cc, 1, 0);
    lv_obj_set_style_radius(s_btn_led_cc, 8, 0);
    lv_obj_set_style_pad_all(s_btn_led_cc, 4, 0);
    lv_obj_clear_flag(s_btn_led_cc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_btn_led_cc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_btn_led_cc, on_led_cc_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_led_ic = lv_label_create(s_btn_led_cc);
    lv_label_set_text(lbl_led_ic, "板载照明");
    lv_obj_set_style_text_font(lbl_led_ic, font_cn, 0);
    lv_obj_set_style_text_color(lbl_led_ic, lv_color_hex(0x34D399), 0);
    lv_obj_align(lbl_led_ic, LV_ALIGN_TOP_LEFT, 2, 2);

    s_lbl_led_cc = lv_label_create(s_btn_led_cc);
    lv_label_set_text(s_lbl_led_cc, led_on ? "照明: 开启" : "照明: 关闭");
    lv_obj_set_style_text_font(s_lbl_led_cc, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_led_cc, lv_color_hex(0xCBD5E1), 0);
    lv_obj_align(s_lbl_led_cc, LV_ALIGN_BOTTOM_LEFT, 2, -2);

    /* ── Wi-Fi 状态卡 ─────────────────────────────────────────────── */
    lv_obj_t *wifi_card = lv_obj_create(s_cc_panel);
    lv_obj_set_size(wifi_card, 108, 48);
    lv_obj_set_pos(wifi_card, 118, 84);
    lv_obj_set_style_bg_color(wifi_card, lv_color_hex(0x0B1A2E), 0);
    lv_obj_set_style_border_color(wifi_card, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_border_width(wifi_card, 1, 0);
    lv_obj_set_style_radius(wifi_card, 8, 0);
    lv_obj_set_style_pad_all(wifi_card, 4, 0);
    lv_obj_clear_flag(wifi_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_wifi_title = lv_label_create(wifi_card);
    lv_label_set_text(lbl_wifi_title, "网络状态");
    lv_obj_set_style_text_font(lbl_wifi_title, font_cn, 0);
    lv_obj_set_style_text_color(lbl_wifi_title, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(lbl_wifi_title, LV_ALIGN_TOP_LEFT, 2, 2);

    s_lbl_wifi_cc = lv_label_create(wifi_card);
    lv_obj_set_width(s_lbl_wifi_cc, 100);
    lv_label_set_long_mode(s_lbl_wifi_cc, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_lbl_wifi_cc, "检测中...");
    lv_obj_set_style_text_font(s_lbl_wifi_cc, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_wifi_cc, lv_color_hex(0xCBD5E1), 0);
    lv_obj_align(s_lbl_wifi_cc, LV_ALIGN_BOTTOM_LEFT, 2, -2);

    /* ── 系统信息横条 ─────────────────────────────────────────────── */
    lv_obj_t *sys_card = lv_obj_create(s_cc_panel);
    lv_obj_set_size(sys_card, 228, 34);
    lv_obj_set_pos(sys_card, 6, 136);
    lv_obj_set_style_bg_color(sys_card, lv_color_hex(0x0A1220), 0);
    lv_obj_set_style_border_color(sys_card, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(sys_card, 1, 0);
    lv_obj_set_style_radius(sys_card, 8, 0);
    lv_obj_set_style_pad_all(sys_card, 4, 0);
    lv_obj_clear_flag(sys_card, LV_OBJ_FLAG_SCROLLABLE);

    s_lbl_heap_cc = lv_label_create(sys_card);
    lv_obj_set_width(s_lbl_heap_cc, 220);
    lv_label_set_long_mode(s_lbl_heap_cc, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_lbl_heap_cc, "ESP32 · 双核 240MHz · 2MB PSRAM");
    lv_obj_set_style_text_font(s_lbl_heap_cc, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_heap_cc, lv_color_hex(0x64748B), 0);
    lv_obj_center(s_lbl_heap_cc);

    ESP_LOGI(TAG, "控制中心 UI 初始化完毕");
}
