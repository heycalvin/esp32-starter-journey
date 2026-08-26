#include "ui_hub.h"
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "bsp_lvgl_port.h"
#include "bsp_display.h"
#include "bsp_led.h"
#include "file_reader.h"
#include "sys_font_manager.h"
#include "ui_novel_reader.h"
#include "ui_photo_album.h"
#include "ui_analytics.h"
#include "ui_pomodoro.h"
#include "ui_game_2048.h"
#include "net_manager.h"
#include "bsp_sdcard.h"
#include "esp_log.h"

static const char *TAG = "UI_HUB";

static lv_obj_t *s_tileview = NULL;

// ── Tile (1, 1) 主桌面看板 (Home Cyber Bento) 控件 ──
static lv_obj_t *s_label_wifi_icon = NULL;
static lv_obj_t *s_label_top_net = NULL;
static lv_obj_t *s_label_top_sd = NULL;
static lv_obj_t *s_label_top_heap = NULL;

static lv_obj_t *s_label_clock = NULL;
static lv_obj_t *s_label_date = NULL;

static lv_obj_t *s_label_bento_temp = NULL;
static lv_obj_t *s_label_bento_humi = NULL;
static lv_obj_t *s_label_bento_dist = NULL;
static lv_obj_t *s_bar_bento_dist = NULL;

static lv_obj_t *s_btn_bento_led = NULL;
static lv_obj_t *s_label_bento_led = NULL;
static lv_obj_t *s_label_bento_ip = NULL;

// ── Tile (1, 0) 下拉控制中心 控件 ──
static lv_obj_t *s_slider_bright = NULL;
static lv_obj_t *s_lbl_bright_val = NULL;
static lv_obj_t *s_btn_cc_led = NULL;
static lv_obj_t *s_lbl_cc_led = NULL;
static lv_obj_t *s_lbl_cc_wifi = NULL;

// ── Tile (5, 1) 系统运维卡片 控件 ──
static lv_obj_t *s_label_sys_uptime = NULL;
static lv_obj_t *s_label_sys_chip = NULL;
static lv_obj_t *s_label_sys_heap = NULL;
static lv_obj_t *s_label_sys_psram = NULL;
static lv_obj_t *s_label_ota = NULL;

/* ── LED 状态双向同步 ── */
static void update_led_button_ui(bool state)
{
    if (state) {
        if (s_label_bento_led) lv_label_set_text(s_label_bento_led, "照明: 开启");
        if (s_btn_bento_led) {
            lv_obj_set_style_bg_color(s_btn_bento_led, lv_color_hex(0x065F46), 0);
            lv_obj_set_style_border_color(s_btn_bento_led, lv_color_hex(0x10B981), 0);
        }
        if (s_lbl_cc_led) lv_label_set_text(s_lbl_cc_led, "照明: 开启");
        if (s_btn_cc_led) {
            lv_obj_set_style_bg_color(s_btn_cc_led, lv_color_hex(0x065F46), 0);
            lv_obj_set_style_border_color(s_btn_cc_led, lv_color_hex(0x10B981), 0);
        }
    } else {
        if (s_label_bento_led) lv_label_set_text(s_label_bento_led, "照明: 关闭");
        if (s_btn_bento_led) {
            lv_obj_set_style_bg_color(s_btn_bento_led, lv_color_hex(0x1E293B), 0);
            lv_obj_set_style_border_color(s_btn_bento_led, lv_color_hex(0x334155), 0);
        }
        if (s_lbl_cc_led) lv_label_set_text(s_lbl_cc_led, "照明: 关闭");
        if (s_btn_cc_led) {
            lv_obj_set_style_bg_color(s_btn_cc_led, lv_color_hex(0x1E293B), 0);
            lv_obj_set_style_border_color(s_btn_cc_led, lv_color_hex(0x334155), 0);
        }
    }
}

static void on_led_btn_clicked(lv_event_t *e)
{
    bsp_led_toggle();
    bool state = bsp_led_get_state();
    update_led_button_ui(state);
}

/* ── 控制中心亮度滑块 ── */
static void on_cc_brightness_changed(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    bsp_display_set_backlight_pwm((uint8_t)val);
    if (s_lbl_bright_val) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", (int)val);
        lv_label_set_text(s_lbl_bright_val, buf);
    }
}

/* ── 快捷返回主页 ── */
static void on_return_home_clicked(lv_event_t *e)
{
    if (s_tileview) {
        lv_tileview_set_tile_by_index(s_tileview, 1, 1, LV_ANIM_ON);
    }
}

/* ── 重置 Wi-Fi 事件 ── */
static void on_reset_wifi_btn_clicked(lv_event_t *e)
{
    ESP_LOGW(TAG, "🔘 用户点击屏幕【重置 Wi-Fi】按钮");
    net_manager_reset_credentials();
}

/* ── 重启系统事件 ── */
static void on_reboot_system_btn_clicked(lv_event_t *e)
{
    ESP_LOGW(TAG, "🔘 用户点击屏幕【重启系统】按钮");
    esp_restart();
}

/* =========================================================================
 * 初始化多维手势滑动系统 (LVGL 2D TileView Matrix)
 * ========================================================================= */
void ui_hub_init(void)
{
    bsp_lvgl_port_lock(0);

    const lv_font_t *font_cn = sys_font_manager_get_font(14);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0A0F1D), 0); // 宇宙黑背景

    // 1. 创建全屏 2D 多维手势滑动容器 (TileView: 240x280 全屏沉浸)
    s_tileview = lv_tileview_create(scr);
    lv_obj_set_size(s_tileview, 240, 280);
    lv_obj_set_style_bg_color(s_tileview, lv_color_hex(0x0A0F1D), 0);
    lv_obj_set_scrollbar_mode(s_tileview, LV_SCROLLBAR_MODE_OFF);

    /* ── 2D 空间矩阵布局 ─────────────────────────────────────────
     *                  (1, 0) 下拉: 🎛️ 控制中心
     * (0, 1) 右滑: ⏱️ 番茄钟  ◄── (1, 1) 🏠 主桌面看板 ──► (2, 1) 相册 ──► (3, 1) 小说 ──► (4, 1) 游戏 ──► (5, 1) 系统
     *                  (1, 2) 上滑: 📈 传感器趋势
     * ──────────────────────────────────────────────────────────── */

    // (1, 1) 中心：主看板 (四向均可滑动)
    lv_obj_t *tile_home = lv_tileview_add_tile(s_tileview, 1, 1, LV_DIR_ALL);

    // (1, 0) 顶部：下拉控制中心 (支持向下滑/向上滑)
    lv_obj_t *tile_cc = lv_tileview_add_tile(s_tileview, 1, 0, (lv_dir_t)(LV_DIR_TOP | LV_DIR_BOTTOM));

    // (1, 2) 底部：上滑传感器折线图 (支持向上滑/向下滑)
    lv_obj_t *tile_analytics = lv_tileview_add_tile(s_tileview, 1, 2, (lv_dir_t)(LV_DIR_TOP | LV_DIR_BOTTOM));

    // (0, 1) 左侧：专注番茄钟 (支持左右滑动)
    lv_obj_t *tile_pomodoro = lv_tileview_add_tile(s_tileview, 0, 1, (lv_dir_t)(LV_DIR_LEFT | LV_DIR_RIGHT));

    // (2, 1) 右侧1：相册
    lv_obj_t *tile_photo = lv_tileview_add_tile(s_tileview, 2, 1, (lv_dir_t)(LV_DIR_LEFT | LV_DIR_RIGHT));

    // (3, 1) 右侧2：小说
    lv_obj_t *tile_novel = lv_tileview_add_tile(s_tileview, 3, 1, (lv_dir_t)(LV_DIR_LEFT | LV_DIR_RIGHT));

    // (4, 1) 右侧3：2048 游戏
    lv_obj_t *tile_game = lv_tileview_add_tile(s_tileview, 4, 1, (lv_dir_t)(LV_DIR_LEFT | LV_DIR_RIGHT));

    // (5, 1) 右侧4：系统运维
    lv_obj_t *tile_sys = lv_tileview_add_tile(s_tileview, 5, 1, (lv_dir_t)(LV_DIR_LEFT | LV_DIR_RIGHT));

    // 统一样式处理
    lv_obj_t *all_tiles[] = {tile_home, tile_cc, tile_analytics, tile_pomodoro, tile_photo, tile_novel, tile_game, tile_sys};
    for (int i = 0; i < 8; i++) {
        lv_obj_set_style_pad_all(all_tiles[i], 4, 0);
        lv_obj_set_style_bg_color(all_tiles[i], lv_color_hex(0x0A0F1D), 0);
        lv_obj_clear_flag(all_tiles[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(all_tiles[i], LV_SCROLLBAR_MODE_OFF);
    }

    /* =========================================================================
     * 🏠 (1, 1) 主桌面看板 (Cyber Bento Dashboard)
     * ========================================================================= */

    // 1. 顶部极简状态胶囊栏 (Y: 2 ~ 18)
    s_label_wifi_icon = lv_label_create(tile_home);
    lv_label_set_text(s_label_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(s_label_wifi_icon, lv_color_hex(0x10B981), 0);
    lv_obj_align(s_label_wifi_icon, LV_ALIGN_TOP_LEFT, 2, 2);

    s_label_top_net = lv_label_create(tile_home);
    lv_label_set_text(s_label_top_net, "WiFi在线");
    lv_obj_set_style_text_font(s_label_top_net, font_cn, 0);
    lv_obj_set_style_text_color(s_label_top_net, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(s_label_top_net, LV_ALIGN_TOP_LEFT, 20, 2);

    s_label_top_sd = lv_label_create(tile_home);
    lv_label_set_text(s_label_top_sd, LV_SYMBOL_SD_CARD " SD在线");
    lv_obj_set_style_text_font(s_label_top_sd, font_cn, 0);
    lv_obj_set_style_text_color(s_label_top_sd, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(s_label_top_sd, LV_ALIGN_TOP_MID, 16, 2);

    s_label_top_heap = lv_label_create(tile_home);
    lv_label_set_text(s_label_top_heap, "180K");
    lv_obj_set_style_text_font(s_label_top_heap, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_label_top_heap, lv_color_hex(0x64748B), 0);
    lv_obj_align(s_label_top_heap, LV_ALIGN_TOP_RIGHT, -2, 2);

    // 2. 上半区：数字时钟、公历、定位与天气卡 (Y: 22, 宽228, 高118)
    lv_obj_t *clock_hero_card = lv_obj_create(tile_home);
    lv_obj_set_size(clock_hero_card, 228, 118);
    lv_obj_align(clock_hero_card, LV_ALIGN_TOP_MID, 0, 22);
    lv_obj_set_style_bg_color(clock_hero_card, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(clock_hero_card, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(clock_hero_card, 1, 0);
    lv_obj_set_style_radius(clock_hero_card, 12, 0);
    lv_obj_set_style_pad_all(clock_hero_card, 4, 0);
    lv_obj_clear_flag(clock_hero_card, LV_OBJ_FLAG_SCROLLABLE);

    // 大号发光数字时钟 (HH:MM:SS)
    s_label_clock = lv_label_create(clock_hero_card);
    lv_label_set_text(s_label_clock, "16:00:00");
    lv_obj_set_style_text_font(s_label_clock, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_label_clock, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(s_label_clock, LV_ALIGN_TOP_MID, 0, 2);

    // 公历年月日与星期
    s_label_date = lv_label_create(clock_hero_card);
    lv_label_set_text(s_label_date, "2026年08月26日 星期三");
    lv_obj_set_style_text_font(s_label_date, font_cn, 0);
    lv_obj_set_style_text_color(s_label_date, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(s_label_date, LV_ALIGN_TOP_MID, 0, 36);

    // 📍 社区与小区高精度位置胶囊条
    lv_obj_t *badge_loc = lv_obj_create(clock_hero_card);
    lv_obj_set_size(badge_loc, 218, 22);
    lv_obj_align(badge_loc, LV_ALIGN_TOP_MID, 0, 58);
    lv_obj_set_style_bg_color(badge_loc, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(badge_loc, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(badge_loc, 1, 0);
    lv_obj_set_style_radius(badge_loc, 6, 0);
    lv_obj_set_style_pad_all(badge_loc, 0, 0);
    lv_obj_clear_flag(badge_loc, LV_OBJ_FLAG_SCROLLABLE);

    s_label_bento_dist = lv_label_create(badge_loc);
    lv_obj_set_width(s_label_bento_dist, 200);
    lv_label_set_long_mode(s_label_bento_dist, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_label_bento_dist, "深圳市 · 南山区");
    lv_obj_set_style_text_font(s_label_bento_dist, font_cn, 0);
    lv_obj_set_style_text_color(s_label_bento_dist, lv_color_hex(0xFBBF24), 0);
    lv_obj_set_style_text_align(s_label_bento_dist, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_label_bento_dist);

    // 实时天气描述胶囊
    s_bar_bento_dist = lv_label_create(clock_hero_card);
    lv_obj_set_width((lv_obj_t *)s_bar_bento_dist, 218);
    lv_label_set_long_mode((lv_obj_t *)s_bar_bento_dist, LV_LABEL_LONG_DOT);
    lv_label_set_text((lv_obj_t *)s_bar_bento_dist, "晴朗 26.5°C · 空气优");
    lv_obj_set_style_text_font((lv_obj_t *)s_bar_bento_dist, font_cn, 0);
    lv_obj_set_style_text_color((lv_obj_t *)s_bar_bento_dist, lv_color_hex(0x34D399), 0);
    lv_obj_set_style_text_align((lv_obj_t *)s_bar_bento_dist, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align((lv_obj_t *)s_bar_bento_dist, LV_ALIGN_BOTTOM_MID, 0, -2);

    // 3. 下半区：双 Bento 科技卡片 (Y: 144, 高 106)
    lv_obj_t *bento_card1 = lv_obj_create(tile_home);
    lv_obj_set_size(bento_card1, 111, 106);
    lv_obj_align(bento_card1, LV_ALIGN_TOP_LEFT, 2, 144);
    lv_obj_set_style_bg_color(bento_card1, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(bento_card1, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(bento_card1, 1, 0);
    lv_obj_set_style_radius(bento_card1, 10, 0);
    lv_obj_set_style_pad_all(bento_card1, 6, 0);
    lv_obj_clear_flag(bento_card1, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bento1_title = lv_label_create(bento_card1);
    lv_label_set_text(bento1_title, "室内环境感知");
    lv_obj_set_style_text_font(bento1_title, font_cn, 0);
    lv_obj_set_style_text_color(bento1_title, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(bento1_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_label_bento_temp = lv_label_create(bento_card1);
    lv_label_set_text(s_label_bento_temp, "26.8°C");
    lv_obj_set_style_text_font(s_label_bento_temp, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_label_bento_temp, lv_color_hex(0xF87171), 0);
    lv_obj_align(s_label_bento_temp, LV_ALIGN_TOP_LEFT, 0, 24);

    s_label_bento_humi = lv_label_create(bento_card1);
    lv_label_set_text(s_label_bento_humi, "湿度: 58.0%");
    lv_obj_set_style_text_font(s_label_bento_humi, font_cn, 0);
    lv_obj_set_style_text_color(s_label_bento_humi, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(s_label_bento_humi, LV_ALIGN_TOP_LEFT, 0, 52);

    lv_obj_t *lbl_air_q = lv_label_create(bento_card1);
    lv_label_set_text(lbl_air_q, "环境评级: 舒适");
    lv_obj_set_style_text_font(lbl_air_q, font_cn, 0);
    lv_obj_set_style_text_color(lbl_air_q, lv_color_hex(0x10B981), 0);
    lv_obj_align(lbl_air_q, LV_ALIGN_TOP_LEFT, 0, 74);

    // Bento 2 (右侧): 快捷照明与网络连接卡片
    lv_obj_t *bento_card2 = lv_obj_create(tile_home);
    lv_obj_set_size(bento_card2, 111, 106);
    lv_obj_align(bento_card2, LV_ALIGN_TOP_RIGHT, -2, 144);
    lv_obj_set_style_bg_color(bento_card2, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(bento_card2, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(bento_card2, 1, 0);
    lv_obj_set_style_radius(bento_card2, 10, 0);
    lv_obj_set_style_pad_all(bento_card2, 6, 0);
    lv_obj_clear_flag(bento_card2, LV_OBJ_FLAG_SCROLLABLE);

    s_btn_bento_led = lv_button_create(bento_card2);
    lv_obj_set_size(s_btn_bento_led, 99, 36);
    lv_obj_align(s_btn_bento_led, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s_btn_bento_led, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(s_btn_bento_led, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(s_btn_bento_led, 1, 0);
    lv_obj_set_style_radius(s_btn_bento_led, 8, 0);
    lv_obj_add_event_cb(s_btn_bento_led, on_led_btn_clicked, LV_EVENT_CLICKED, NULL);

    s_label_bento_led = lv_label_create(s_btn_bento_led);
    lv_label_set_text(s_label_bento_led, "照明: 关闭");
    lv_obj_set_style_text_font(s_label_bento_led, font_cn, 0);
    lv_obj_set_style_text_color(s_label_bento_led, lv_color_hex(0xCBD5E1), 0);
    lv_obj_center(s_label_bento_led);

    s_label_bento_ip = lv_label_create(bento_card2);
    lv_obj_set_width(s_label_bento_ip, 99);
    lv_label_set_long_mode(s_label_bento_ip, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_label_bento_ip, "网络: 192.168.4.1");
    lv_obj_set_style_text_font(s_label_bento_ip, font_cn, 0);
    lv_obj_set_style_text_color(s_label_bento_ip, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(s_label_bento_ip, LV_ALIGN_TOP_LEFT, 0, 44);

    lv_obj_t *lbl_ap_mode = lv_label_create(bento_card2);
    lv_obj_set_width(lbl_ap_mode, 99);
    lv_label_set_long_mode(lbl_ap_mode, LV_LABEL_LONG_DOT);
    lv_label_set_text(lbl_ap_mode, "热点: ESP32-Hub");
    lv_obj_set_style_text_font(lbl_ap_mode, font_cn, 0);
    lv_obj_set_style_text_color(lbl_ap_mode, lv_color_hex(0x64748B), 0);
    lv_obj_align(lbl_ap_mode, LV_ALIGN_TOP_LEFT, 0, 74);

    // 4. 底部微光滑动导航提示
    lv_obj_t *lbl_nav_hint = lv_label_create(tile_home);
    lv_label_set_text(lbl_nav_hint, "▼下拉设置  ▲上滑传感  ◀番茄钟  ▶应用");
    lv_obj_set_style_text_font(lbl_nav_hint, font_cn, 0);
    lv_obj_set_style_text_color(lbl_nav_hint, lv_color_hex(0x334155), 0);
    lv_obj_align(lbl_nav_hint, LV_ALIGN_BOTTOM_MID, 0, -2);

    /* =========================================================================
     * 🎛️ (1, 0) 下拉控制中心 (Top Control Center)
     * ========================================================================= */
    // 标题栏
    lv_obj_t *cc_top_bar = lv_obj_create(tile_cc);
    lv_obj_set_size(cc_top_bar, 228, 30);
    lv_obj_set_pos(cc_top_bar, 2, 2);
    lv_obj_set_style_bg_color(cc_top_bar, lv_color_hex(0x0F1B2D), 0);
    lv_obj_set_style_border_width(cc_top_bar, 0, 0);
    lv_obj_set_style_radius(cc_top_bar, 8, 0);
    lv_obj_set_style_pad_all(cc_top_bar, 2, 0);
    lv_obj_clear_flag(cc_top_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_cc_title = lv_label_create(cc_top_bar);
    lv_label_set_text(lbl_cc_title, "🎛️ 控制中心");
    lv_obj_set_style_text_font(lbl_cc_title, font_cn, 0);
    lv_obj_set_style_text_color(lbl_cc_title, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(lbl_cc_title, LV_ALIGN_LEFT_MID, 6, 0);

    lv_obj_t *btn_cc_back = lv_button_create(cc_top_bar);
    lv_obj_set_size(btn_cc_back, 64, 22);
    lv_obj_align(btn_cc_back, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(btn_cc_back, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_radius(btn_cc_back, 6, 0);
    lv_obj_add_event_cb(btn_cc_back, on_return_home_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_btn_bk = lv_label_create(btn_cc_back);
    lv_label_set_text(lbl_btn_bk, "▲ 返回");
    lv_obj_set_style_text_font(lbl_btn_bk, font_cn, 0);
    lv_obj_center(lbl_btn_bk);

    // 亮度卡片 (Y: 36, 高 50)
    lv_obj_t *cc_bright_card = lv_obj_create(tile_cc);
    lv_obj_set_size(cc_bright_card, 228, 50);
    lv_obj_set_pos(cc_bright_card, 2, 36);
    lv_obj_set_style_bg_color(cc_bright_card, lv_color_hex(0x0F1B2D), 0);
    lv_obj_set_style_border_color(cc_bright_card, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_border_width(cc_bright_card, 1, 0);
    lv_obj_set_style_radius(cc_bright_card, 8, 0);
    lv_obj_set_style_pad_all(cc_bright_card, 4, 0);
    lv_obj_clear_flag(cc_bright_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_bl_name = lv_label_create(cc_bright_card);
    lv_label_set_text(lbl_bl_name, "屏幕亮度 (PWM 调光)");
    lv_obj_set_style_text_font(lbl_bl_name, font_cn, 0);
    lv_obj_set_style_text_color(lbl_bl_name, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(lbl_bl_name, LV_ALIGN_TOP_LEFT, 4, 0);

    s_lbl_bright_val = lv_label_create(cc_bright_card);
    char br_buf[16];
    snprintf(br_buf, sizeof(br_buf), "%d%%", bsp_display_get_backlight_pct());
    lv_label_set_text(s_lbl_bright_val, br_buf);
    lv_obj_set_style_text_font(s_lbl_bright_val, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_bright_val, lv_color_hex(0xFBBF24), 0);
    lv_obj_align(s_lbl_bright_val, LV_ALIGN_TOP_RIGHT, -4, 0);

    s_slider_bright = lv_slider_create(cc_bright_card);
    lv_obj_set_size(s_slider_bright, 216, 12);
    lv_obj_align(s_slider_bright, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_slider_set_range(s_slider_bright, 10, 100);
    lv_slider_set_value(s_slider_bright, bsp_display_get_backlight_pct(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_slider_bright, lv_color_hex(0x1E293B), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider_bright, lv_color_hex(0x38BDF8), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider_bright, lv_color_hex(0xF8FAFC), LV_PART_KNOB);
    lv_obj_add_event_cb(s_slider_bright, on_cc_brightness_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // 照明与网络双卡 (Y: 90, 高 58)
    s_btn_cc_led = lv_obj_create(tile_cc);
    lv_obj_set_size(s_btn_cc_led, 110, 58);
    lv_obj_set_pos(s_btn_cc_led, 2, 90);
    bool led_on = bsp_led_get_state();
    lv_obj_set_style_bg_color(s_btn_cc_led, led_on ? lv_color_hex(0x065F46) : lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(s_btn_cc_led, led_on ? lv_color_hex(0x10B981) : lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(s_btn_cc_led, 1, 0);
    lv_obj_set_style_radius(s_btn_cc_led, 8, 0);
    lv_obj_set_style_pad_all(s_btn_cc_led, 4, 0);
    lv_obj_add_flag(s_btn_cc_led, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_btn_cc_led, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_btn_cc_led, on_led_btn_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_ic_led = lv_label_create(s_btn_cc_led);
    lv_label_set_text(lbl_ic_led, "板载照明");
    lv_obj_set_style_text_font(lbl_ic_led, font_cn, 0);
    lv_obj_set_style_text_color(lbl_ic_led, lv_color_hex(0x34D399), 0);
    lv_obj_align(lbl_ic_led, LV_ALIGN_TOP_LEFT, 2, 2);

    s_lbl_cc_led = lv_label_create(s_btn_cc_led);
    lv_label_set_text(s_lbl_cc_led, led_on ? "照明: 开启" : "照明: 关闭");
    lv_obj_set_style_text_font(s_lbl_cc_led, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_cc_led, lv_color_hex(0xCBD5E1), 0);
    lv_obj_align(s_lbl_cc_led, LV_ALIGN_BOTTOM_LEFT, 2, -2);

    // Wi-Fi 状态卡
    lv_obj_t *cc_wifi_card = lv_obj_create(tile_cc);
    lv_obj_set_size(cc_wifi_card, 114, 58);
    lv_obj_set_pos(cc_wifi_card, 116, 90);
    lv_obj_set_style_bg_color(cc_wifi_card, lv_color_hex(0x0F1B2D), 0);
    lv_obj_set_style_border_color(cc_wifi_card, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_border_width(cc_wifi_card, 1, 0);
    lv_obj_set_style_radius(cc_wifi_card, 8, 0);
    lv_obj_set_style_pad_all(cc_wifi_card, 4, 0);
    lv_obj_clear_flag(cc_wifi_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_wf_title = lv_label_create(cc_wifi_card);
    lv_label_set_text(lbl_wf_title, "网络连接");
    lv_obj_set_style_text_font(lbl_wf_title, font_cn, 0);
    lv_obj_set_style_text_color(lbl_wf_title, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(lbl_wf_title, LV_ALIGN_TOP_LEFT, 2, 2);

    s_lbl_cc_wifi = lv_label_create(cc_wifi_card);
    lv_obj_set_width(s_lbl_cc_wifi, 104);
    lv_label_set_long_mode(s_lbl_cc_wifi, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_lbl_cc_wifi, "WiFi 在线");
    lv_obj_set_style_text_font(s_lbl_cc_wifi, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_cc_wifi, lv_color_hex(0xCBD5E1), 0);
    lv_obj_align(s_lbl_cc_wifi, LV_ALIGN_BOTTOM_LEFT, 2, -2);

    // 硬件信息横条 (Y: 152, 高 40)
    lv_obj_t *cc_hw_card = lv_obj_create(tile_cc);
    lv_obj_set_size(cc_hw_card, 228, 40);
    lv_obj_set_pos(cc_hw_card, 2, 152);
    lv_obj_set_style_bg_color(cc_hw_card, lv_color_hex(0x0A1220), 0);
    lv_obj_set_style_border_color(cc_hw_card, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(cc_hw_card, 1, 0);
    lv_obj_set_style_radius(cc_hw_card, 8, 0);
    lv_obj_set_style_pad_all(cc_hw_card, 4, 0);
    lv_obj_clear_flag(cc_hw_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_hw_info = lv_label_create(cc_hw_card);
    lv_label_set_text(lbl_hw_info, "ESP32 · 240MHz · 2MB PSRAM · 8MB Flash");
    lv_obj_set_style_text_font(lbl_hw_info, font_cn, 0);
    lv_obj_set_style_text_color(lbl_hw_info, lv_color_hex(0x64748B), 0);
    lv_obj_center(lbl_hw_info);

    // 底部双运维动作按钮 (Y: 196, 高 36)
    lv_obj_t *btn_cc_reset = lv_button_create(tile_cc);
    lv_obj_set_size(btn_cc_reset, 110, 36);
    lv_obj_set_pos(btn_cc_reset, 2, 196);
    lv_obj_set_style_bg_color(btn_cc_reset, lv_color_hex(0x991B1B), 0);
    lv_obj_set_style_radius(btn_cc_reset, 8, 0);
    lv_obj_add_event_cb(btn_cc_reset, on_reset_wifi_btn_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_cc_rw = lv_label_create(btn_cc_reset);
    lv_label_set_text(lbl_cc_rw, "配网重置");
    lv_obj_set_style_text_font(lbl_cc_rw, font_cn, 0);
    lv_obj_center(lbl_cc_rw);

    lv_obj_t *btn_cc_reboot = lv_button_create(tile_cc);
    lv_obj_set_size(btn_cc_reboot, 114, 36);
    lv_obj_set_pos(btn_cc_reboot, 116, 196);
    lv_obj_set_style_bg_color(btn_cc_reboot, lv_color_hex(0x0369A1), 0);
    lv_obj_set_style_radius(btn_cc_reboot, 8, 0);
    lv_obj_add_event_cb(btn_cc_reboot, on_reboot_system_btn_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_cc_rb = lv_label_create(btn_cc_reboot);
    lv_label_set_text(lbl_cc_rb, "重启系统");
    lv_obj_set_style_text_font(lbl_cc_rb, font_cn, 0);
    lv_obj_center(lbl_cc_rb);

    // 底部滑动指引
    lv_obj_t *lbl_cc_hint = lv_label_create(tile_cc);
    lv_label_set_text(lbl_cc_hint, "▲ 上滑返回主页");
    lv_obj_set_style_text_font(lbl_cc_hint, font_cn, 0);
    lv_obj_set_style_text_color(lbl_cc_hint, lv_color_hex(0x475569), 0);
    lv_obj_align(lbl_cc_hint, LV_ALIGN_BOTTOM_MID, 0, -4);

    /* =========================================================================
     * 📈 (1, 2) 上滑传感器 24H 折线图 (Bottom Analytics)
     * ========================================================================= */
    ui_analytics_init(tile_analytics);

    /* =========================================================================
     * ⏱️ (0, 1) 右滑专注番茄钟 / 大号时钟 (Left Pomodoro)
     * ========================================================================= */
    ui_pomodoro_init(tile_pomodoro);

    /* =========================================================================
     * 🖼️ (2, 1) 左滑相册 (Right 1 Photo Album)
     * ========================================================================= */
    ui_photo_album_init(tile_photo);

    /* =========================================================================
     * 📖 (3, 1) 继续左滑小说 (Right 2 Novel Reader)
     * ========================================================================= */
    ui_novel_reader_init(tile_novel);

    /* =========================================================================
     * 🎮 (4, 1) 继续左滑 2048 触控游戏 (Right 3 2048 Game)
     * ========================================================================= */
    ui_game_2048_init(tile_game);

    /* =========================================================================
     * ⚙️ (5, 1) 继续左滑系统运维看板 (Right 4 System Settings)
     * ========================================================================= */
    // 标题卡
    lv_obj_t *sys_hero = lv_obj_create(tile_sys);
    lv_obj_set_size(sys_hero, 228, 44);
    lv_obj_set_pos(sys_hero, 2, 2);
    lv_obj_set_style_bg_color(sys_hero, lv_color_hex(0x0F1B2D), 0);
    lv_obj_set_style_border_color(sys_hero, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_border_width(sys_hero, 1, 0);
    lv_obj_set_style_radius(sys_hero, 8, 0);
    lv_obj_set_style_pad_all(sys_hero, 4, 0);
    lv_obj_clear_flag(sys_hero, LV_OBJ_FLAG_SCROLLABLE);

    s_label_sys_uptime = lv_label_create(sys_hero);
    lv_label_set_text(s_label_sys_uptime, "心跳: 00:00:00 (在线)");
    lv_obj_set_style_text_font(s_label_sys_uptime, font_cn, 0);
    lv_obj_set_style_text_color(s_label_sys_uptime, lv_color_hex(0x10B981), 0);
    lv_obj_align(s_label_sys_uptime, LV_ALIGN_TOP_LEFT, 4, 2);

    lv_obj_t *lbl_sys_ver = lv_label_create(sys_hero);
    lv_label_set_text(lbl_sys_ver, "ESP-IDF v6.0 · FreeRTOS");
    lv_obj_set_style_text_font(lbl_sys_ver, font_cn, 0);
    lv_obj_set_style_text_color(lbl_sys_ver, lv_color_hex(0x64748B), 0);
    lv_obj_align(lbl_sys_ver, LV_ALIGN_BOTTOM_LEFT, 4, -2);

    // 内存与存储卡
    lv_obj_t *sys_mem_card = lv_obj_create(tile_sys);
    lv_obj_set_size(sys_mem_card, 228, 64);
    lv_obj_set_pos(sys_mem_card, 2, 50);
    lv_obj_set_style_bg_color(sys_mem_card, lv_color_hex(0x0B1426), 0);
    lv_obj_set_style_border_color(sys_mem_card, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(sys_mem_card, 1, 0);
    lv_obj_set_style_radius(sys_mem_card, 8, 0);
    lv_obj_set_style_pad_all(sys_mem_card, 4, 0);
    lv_obj_clear_flag(sys_mem_card, LV_OBJ_FLAG_SCROLLABLE);

    s_label_sys_heap = lv_label_create(sys_mem_card);
    lv_label_set_text(s_label_sys_heap, "内部 SRAM: 180 KB 剩余");
    lv_obj_set_style_text_font(s_label_sys_heap, font_cn, 0);
    lv_obj_set_style_text_color(s_label_sys_heap, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(s_label_sys_heap, LV_ALIGN_TOP_LEFT, 4, 2);

    s_label_sys_psram = lv_label_create(sys_mem_card);
    lv_label_set_text(s_label_sys_psram, "外部 PSRAM: 1.82 MB 剩余");
    lv_obj_set_style_text_font(s_label_sys_psram, font_cn, 0);
    lv_obj_set_style_text_color(s_label_sys_psram, lv_color_hex(0x34D399), 0);
    lv_obj_align(s_label_sys_psram, LV_ALIGN_BOTTOM_LEFT, 4, -2);

    // 存储与 OTA 卡
    lv_obj_t *sys_hw_card = lv_obj_create(tile_sys);
    lv_obj_set_size(sys_hw_card, 228, 64);
    lv_obj_set_pos(sys_hw_card, 2, 118);
    lv_obj_set_style_bg_color(sys_hw_card, lv_color_hex(0x0B1426), 0);
    lv_obj_set_style_border_color(sys_hw_card, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(sys_hw_card, 1, 0);
    lv_obj_set_style_radius(sys_hw_card, 8, 0);
    lv_obj_set_style_pad_all(sys_hw_card, 4, 0);
    lv_obj_clear_flag(sys_hw_card, LV_OBJ_FLAG_SCROLLABLE);

    s_label_sys_chip = lv_label_create(sys_hw_card);
    lv_label_set_text(s_label_sys_chip, "存储: TF卡已挂载 (8GB)");
    lv_obj_set_style_text_font(s_label_sys_chip, font_cn, 0);
    lv_obj_set_style_text_color(s_label_sys_chip, lv_color_hex(0xFBBF24), 0);
    lv_obj_align(s_label_sys_chip, LV_ALIGN_TOP_LEFT, 4, 2);

    s_label_ota = lv_label_create(sys_hw_card);
    lv_label_set_text(s_label_ota, "固件: ESP32 · OTA_0 运行");
    lv_obj_set_style_text_font(s_label_ota, font_cn, 0);
    lv_obj_set_style_text_color(s_label_ota, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(s_label_ota, LV_ALIGN_BOTTOM_LEFT, 4, -2);

    // 运维按钮与返回
    lv_obj_t *btn_sys_home = lv_button_create(tile_sys);
    lv_obj_set_size(btn_sys_home, 228, 36);
    lv_obj_set_pos(btn_sys_home, 2, 186);
    lv_obj_set_style_bg_color(btn_sys_home, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_radius(btn_sys_home, 8, 0);
    lv_obj_add_event_cb(btn_sys_home, on_return_home_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_sys_hm = lv_label_create(btn_sys_home);
    lv_label_set_text(lbl_sys_hm, "◀ 返回主桌面");
    lv_obj_set_style_text_font(lbl_sys_hm, font_cn, 0);
    lv_obj_center(lbl_sys_hm);

    // 默认聚焦到 (1, 1) 主桌面看板
    lv_tileview_set_tile_by_index(s_tileview, 1, 1, LV_ANIM_OFF);

    bsp_lvgl_port_unlock();
    ESP_LOGI(TAG, "🚀 [UI] 2D 多维手势滑动交互系统初始化完毕！");
}

/* =========================================================================
 * 业务数据刷新接口
 * ========================================================================= */
void ui_hub_update_sensor_data(const bsp_sensor_data_t *data)
{
    if (!data) return;
    bsp_lvgl_port_lock(0);

    char buf[64];

    // 驱动 Tab 1 Cyber Bento 微卡片组件
    if (s_label_bento_temp) {
        snprintf(buf, sizeof(buf), "%.1f°C", data->ntc_temperature);
        lv_label_set_text(s_label_bento_temp, buf);
    }
    if (s_label_bento_humi) {
        snprintf(buf, sizeof(buf), "湿度: %.1f%%", data->dht_humidity);
        lv_label_set_text(s_label_bento_humi, buf);
    }

    // 推送数据到折线图 (Bottom Tile Analytics)
    ui_analytics_push_data(data->ntc_temperature, data->dht_humidity);

    bsp_lvgl_port_unlock();
}

void ui_hub_update_time_and_date(const char *time_str, const char *date_str)
{
    bsp_lvgl_port_lock(0);
    if (time_str && s_label_clock) {
        lv_label_set_text(s_label_clock, time_str);
    }
    if (date_str && s_label_date) {
        lv_label_set_text(s_label_date, date_str);
    }
    bsp_lvgl_port_unlock();
}

void ui_hub_update_weather_full(const char *location, const char *weather_desc, float temp, float humi)
{
    bsp_lvgl_port_lock(0);
    if (s_label_bento_dist && location) {
        lv_label_set_text(s_label_bento_dist, location);
    }
    if (s_bar_bento_dist && weather_desc) {
        lv_label_set_text((lv_obj_t *)s_bar_bento_dist, weather_desc);
    }
    if (s_label_bento_temp) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f°C", temp);
        lv_label_set_text(s_label_bento_temp, buf);
    }
    if (s_label_bento_humi) {
        char buf[32];
        snprintf(buf, sizeof(buf), "湿度: %.1f%%", humi);
        lv_label_set_text(s_label_bento_humi, buf);
    }
    bsp_lvgl_port_unlock();
}

void ui_hub_update_system_status(const char *uptime_str, const char *ip_str, uint32_t heap, uint32_t psram)
{
    bsp_lvgl_port_lock(0);

    // 1. 顶部状态栏与 Bento 4 网络信息更新
    if (s_label_top_heap) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%luK", (unsigned long)(heap / 1024));
        lv_label_set_text(s_label_top_heap, buf);
    }

    if (s_label_top_sd) {
        if (bsp_sdcard_is_mounted()) {
            lv_label_set_text(s_label_top_sd, LV_SYMBOL_SD_CARD " SD在线");
            lv_obj_set_style_text_color(s_label_top_sd, lv_color_hex(0x38BDF8), 0);
        } else {
            lv_label_set_text(s_label_top_sd, LV_SYMBOL_SD_CARD " 无SD卡");
            lv_obj_set_style_text_color(s_label_top_sd, lv_color_hex(0x64748B), 0);
        }
    }

    if (ip_str && strlen(ip_str) > 0) {
        if (net_manager_is_wifi_connected()) {
            if (s_label_wifi_icon) {
                lv_label_set_text(s_label_wifi_icon, LV_SYMBOL_WIFI);
                lv_obj_set_style_text_color(s_label_wifi_icon, lv_color_hex(0x10B981), 0);
            }
            if (s_label_top_net) {
                lv_label_set_text(s_label_top_net, "WiFi在线");
                lv_obj_set_style_text_color(s_label_top_net, lv_color_hex(0x10B981), 0);
            }
            if (s_label_bento_ip) {
                char buf[64];
                snprintf(buf, sizeof(buf), "网络: %s", ip_str);
                lv_label_set_text(s_label_bento_ip, buf);
            }
            if (s_lbl_cc_wifi) {
                char buf[64];
                snprintf(buf, sizeof(buf), "WiFi: %s", ip_str);
                lv_label_set_text(s_lbl_cc_wifi, buf);
            }
        } else if (net_manager_is_provisioning()) {
            if (s_label_wifi_icon) {
                lv_label_set_text(s_label_wifi_icon, LV_SYMBOL_SETTINGS);
                lv_obj_set_style_text_color(s_label_wifi_icon, lv_color_hex(0xFBBF24), 0);
            }
            if (s_label_top_net) {
                lv_label_set_text(s_label_top_net, "AP配网中");
                lv_obj_set_style_text_color(s_label_top_net, lv_color_hex(0xFBBF24), 0);
            }
            if (s_label_bento_ip) {
                lv_label_set_text(s_label_bento_ip, "配网: 192.168.4.1");
            }
            if (s_lbl_cc_wifi) {
                lv_label_set_text(s_lbl_cc_wifi, "热点: ESP32-Hub");
            }
        } else {
            if (s_label_wifi_icon) {
                lv_label_set_text(s_label_wifi_icon, LV_SYMBOL_WARNING);
                lv_obj_set_style_text_color(s_label_wifi_icon, lv_color_hex(0xEF4444), 0);
            }
            if (s_label_top_net) {
                lv_label_set_text(s_label_top_net, "连接中...");
                lv_obj_set_style_text_color(s_label_top_net, lv_color_hex(0x64748B), 0);
            }
            if (s_label_bento_ip) {
                lv_label_set_text(s_label_bento_ip, "网络: 正在连接 WiFi...");
            }
            if (s_lbl_cc_wifi) {
                lv_label_set_text(s_lbl_cc_wifi, "WiFi: 正在连接...");
            }
        }
    }

    // 2. 系统运维卡片更新
    if (uptime_str && s_label_sys_uptime) {
        char buf[64];
        snprintf(buf, sizeof(buf), "心跳: %s (在线)", uptime_str);
        lv_label_set_text(s_label_sys_uptime, buf);
    }

    if (bsp_sdcard_is_mounted()) {
        if (s_label_sys_chip) {
            uint32_t total_mb = 0, free_mb = 0;
            bsp_sdcard_get_space_mb(&total_mb, &free_mb);
            char sdbuf[64];
            if (total_mb > 0) {
                snprintf(sdbuf, sizeof(sdbuf), "存储: TF卡已挂载 (余 %luM)", (unsigned long)free_mb);
            } else {
                snprintf(sdbuf, sizeof(sdbuf), "存储: TF卡已挂载 (8GB)");
            }
            lv_label_set_text(s_label_sys_chip, sdbuf);
        }
    } else {
        if (s_label_sys_chip) {
            lv_label_set_text(s_label_sys_chip, "存储: 未插 TF 卡 (Flash)");
        }
    }

    if (s_label_sys_heap) {
        char buf[64];
        snprintf(buf, sizeof(buf), "内部 SRAM: %lu KB 剩余", (unsigned long)(heap / 1024));
        lv_label_set_text(s_label_sys_heap, buf);
    }
    if (s_label_sys_psram) {
        char buf[64];
        snprintf(buf, sizeof(buf), "外部 PSRAM: %.2f MB 剩余", (float)psram / (1024.0f * 1024.0f));
        lv_label_set_text(s_label_sys_psram, buf);
    }

    bsp_lvgl_port_unlock();
}

void ui_hub_update_ota_progress(int progress_pct)
{
    bsp_lvgl_port_lock(0);
    if (s_label_ota) {
        char buf[64];
        snprintf(buf, sizeof(buf), "固件升级中: %d%%", progress_pct);
        lv_label_set_text(s_label_ota, buf);
    }
    bsp_lvgl_port_unlock();
}
