#include "ui_hub.h"
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "bsp_lvgl_port.h"
#include "bsp_led.h"
#include "file_reader.h"
#include "sys_font_manager.h"
#include "ui_novel_reader.h"
#include "ui_photo_album.h"
#include "ui_control_center.h"
#include "ui_analytics.h"
#include "ui_pomodoro.h"
#include "ui_game_2048.h"
#include "net_manager.h"
#include "bsp_sdcard.h"
#include "esp_log.h"

static const char *TAG = "UI_HUB";

static lv_obj_t *s_tabview = NULL;

// Tab 1: 桌面极光科技看板 (Cyber Bento Grid) 组件
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
static lv_obj_t *s_label_bento_net_status = NULL;

// Tab 2: 传感器 IoT 仪表组件
static lv_obj_t *s_bar_temp = NULL;
static lv_obj_t *s_label_temp = NULL;
static lv_obj_t *s_bar_humi = NULL;
static lv_obj_t *s_label_humi = NULL;
static lv_obj_t *s_bar_dist = NULL;
static lv_obj_t *s_label_dist = NULL;
static lv_obj_t *s_btn_led = NULL;
static lv_obj_t *s_label_led = NULL;

// Tab 5: 系统设置组件
static lv_obj_t *s_label_sys_uptime = NULL;
static lv_obj_t *s_label_sys_chip = NULL;
static lv_obj_t *s_label_sys_heap = NULL;
static lv_obj_t *s_label_sys_psram = NULL;
static lv_obj_t *s_label_ota = NULL;

/* 按钮事件：LED 翻转 (首页与Tab2双向同步) */
static void update_led_button_ui(bool state)
{
    const lv_font_t *font_cn = sys_font_manager_get_font(14);
    if (state) {
        if (s_label_led) {
            lv_label_set_text(s_label_led, LV_SYMBOL_POWER " 板载照明: 已激活");
            lv_obj_set_style_text_font(s_label_led, font_cn, 0);
        }
        if (s_btn_led) {
            lv_obj_set_style_bg_color(s_btn_led, lv_color_hex(0x064E3B), 0);
            lv_obj_set_style_border_color(s_btn_led, lv_color_hex(0x10B981), 0);
        }
        if (s_label_bento_led) lv_label_set_text(s_label_bento_led, "照明: 开启");
        if (s_btn_bento_led) {
            lv_obj_set_style_bg_color(s_btn_bento_led, lv_color_hex(0x065F46), 0);
            lv_obj_set_style_border_color(s_btn_bento_led, lv_color_hex(0x10B981), 0);
        }
    } else {
        if (s_label_led) {
            lv_label_set_text(s_label_led, LV_SYMBOL_POWER " 板载照明: 已关闭");
            lv_obj_set_style_text_font(s_label_led, font_cn, 0);
        }
        if (s_btn_led) {
            lv_obj_set_style_bg_color(s_btn_led, lv_color_hex(0x1E293B), 0);
            lv_obj_set_style_border_color(s_btn_led, lv_color_hex(0x334155), 0);
        }
        if (s_label_bento_led) lv_label_set_text(s_label_bento_led, "照明: 关闭");
        if (s_btn_bento_led) {
            lv_obj_set_style_bg_color(s_btn_bento_led, lv_color_hex(0x1E293B), 0);
            lv_obj_set_style_border_color(s_btn_bento_led, lv_color_hex(0x334155), 0);
        }
    }
}

static void on_led_btn_clicked(lv_event_t *e)
{
    bsp_led_toggle();
    bool state = bsp_led_get_state();
    update_led_button_ui(state);
}

/* 重置 Wi-Fi 事件 */
static void on_reset_wifi_btn_clicked(lv_event_t *e)
{
    ESP_LOGW(TAG, "🔘 用户点击屏幕【重置 Wi-Fi】按钮");
    net_manager_reset_credentials();
}

/* 重启系统事件 */
static void on_reboot_system_btn_clicked(lv_event_t *e)
{
    ESP_LOGW(TAG, "🔘 用户点击屏幕【重启系统】按钮");
    esp_restart();
}

void ui_hub_init(void)
{
    bsp_lvgl_port_lock(0);

    const lv_font_t *font_cn = sys_font_manager_get_font(14);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0A0F1D), 0); // 宇宙黑背景

    // 1. 创建多应用滑动 TabView，导航栏置于底部 (Bottom Dock)
    s_tabview = lv_tabview_create(scr);
    lv_tabview_set_tab_bar_position(s_tabview, LV_DIR_BOTTOM);
    lv_tabview_set_tab_bar_size(s_tabview, 40);
    lv_obj_set_size(s_tabview, 240, 280);
    lv_obj_set_style_bg_color(s_tabview, lv_color_hex(0x0A0F1D), 0);

    // 底部导航栏样式定制 (暗黑磨砂 + 悬浮指示)
    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(s_tabview);
    lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(tab_bar, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(tab_bar, 1, 0);
    lv_obj_set_style_border_side(tab_bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_pad_all(tab_bar, 2, 0);
    lv_obj_set_style_pad_gap(tab_bar, 2, 0);

    // 8 个 Tab 按钮：主页/传感/相册/阅读/系统/图表/时钟/游戏
    lv_obj_t *tab1 = lv_tabview_add_tab(s_tabview, LV_SYMBOL_HOME);
    lv_obj_t *tab2 = lv_tabview_add_tab(s_tabview, LV_SYMBOL_CHARGE);
    lv_obj_t *tab3 = lv_tabview_add_tab(s_tabview, LV_SYMBOL_IMAGE);
    lv_obj_t *tab4 = lv_tabview_add_tab(s_tabview, LV_SYMBOL_FILE);
    lv_obj_t *tab5 = lv_tabview_add_tab(s_tabview, LV_SYMBOL_SETTINGS);
    lv_obj_t *tab6 = lv_tabview_add_tab(s_tabview, LV_SYMBOL_LOOP);     // 图表
    lv_obj_t *tab7 = lv_tabview_add_tab(s_tabview, LV_SYMBOL_BELL);     // 时钟/番茄钟
    lv_obj_t *tab8 = lv_tabview_add_tab(s_tabview, LV_SYMBOL_PLAY);     // 2048 游戏

    // 消除 Tab 内部默认冗余边距并锁定页面禁止误滚
    lv_obj_t *tabs[] = {tab1, tab2, tab3, tab4, tab5, tab6, tab7, tab8};
    for (int i = 0; i < 8; i++) {
        lv_obj_set_style_pad_all(tabs[i], 4, 0);
        lv_obj_set_style_bg_color(tabs[i], lv_color_hex(0x0A0F1D), 0);
        lv_obj_clear_flag(tabs[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(tabs[i], LV_SCROLLBAR_MODE_OFF);
    }

    /* =========================================================================
     * 🌤️ Tab 1: 桌面极光科技看板 (Cyber Bento Dashboard)
     * ========================================================================= */

    // 1. 顶部极简状态胶囊栏 (Y: 0 ~ 18)
    s_label_wifi_icon = lv_label_create(tab1);
    lv_label_set_text(s_label_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(s_label_wifi_icon, lv_color_hex(0x10B981), 0);
    lv_obj_align(s_label_wifi_icon, LV_ALIGN_TOP_LEFT, 2, 0);

    s_label_top_net = lv_label_create(tab1);
    lv_label_set_text(s_label_top_net, "WiFi在线");
    lv_obj_set_style_text_font(s_label_top_net, font_cn, 0);
    lv_obj_set_style_text_color(s_label_top_net, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(s_label_top_net, LV_ALIGN_TOP_LEFT, 20, 0);

    s_label_top_sd = lv_label_create(tab1);
    lv_label_set_text(s_label_top_sd, LV_SYMBOL_SD_CARD " SD在线");
    lv_obj_set_style_text_font(s_label_top_sd, font_cn, 0);
    lv_obj_set_style_text_color(s_label_top_sd, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(s_label_top_sd, LV_ALIGN_TOP_MID, 16, 0);

    s_label_top_heap = lv_label_create(tab1);
    lv_label_set_text(s_label_top_heap, "180K");
    lv_obj_set_style_text_font(s_label_top_heap, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_label_top_heap, lv_color_hex(0x64748B), 0);
    lv_obj_align(s_label_top_heap, LV_ALIGN_TOP_RIGHT, -2, 0);

    // 2. 上半区：赛博数字时钟、公历、小区社区定位与天气全景卡 (X:0, Y:18, 宽228, 高110)
    lv_obj_t *clock_hero_card = lv_obj_create(tab1);
    lv_obj_set_size(clock_hero_card, 228, 110);
    lv_obj_align(clock_hero_card, LV_ALIGN_TOP_MID, 0, 18);
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
    lv_obj_align(s_label_clock, LV_ALIGN_TOP_MID, 0, 0);

    // 公历年月日与星期
    s_label_date = lv_label_create(clock_hero_card);
    lv_label_set_text(s_label_date, "2026年08月26日 星期三");
    lv_obj_set_style_text_font(s_label_date, font_cn, 0);
    lv_obj_set_style_text_color(s_label_date, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(s_label_date, LV_ALIGN_TOP_MID, 0, 32);

    // 📍 社区与小区高精度位置胶囊条 (Location Badge)
    lv_obj_t *badge_loc = lv_obj_create(clock_hero_card);
    lv_obj_set_size(badge_loc, 218, 22);
    lv_obj_align(badge_loc, LV_ALIGN_TOP_MID, 0, 54);
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

    // 3. 下半区：呼吸感开阔双 Bento 卡片 (Y: 134 ~ 222, 高 88)

    // Bento 1 (左侧): 室内温湿度感知卡片 (宽111, 高88)
    lv_obj_t *bento_card1 = lv_obj_create(tab1);
    lv_obj_set_size(bento_card1, 111, 88);
    lv_obj_align(bento_card1, LV_ALIGN_TOP_LEFT, 0, 134);
    lv_obj_set_style_bg_color(bento_card1, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(bento_card1, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(bento_card1, 1, 0);
    lv_obj_set_style_radius(bento_card1, 10, 0);
    lv_obj_set_style_pad_all(bento_card1, 6, 0);
    lv_obj_clear_flag(bento_card1, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bento1_title = lv_label_create(bento_card1);
    lv_label_set_text(bento1_title, "室内环境感知");
    lv_obj_set_style_text_font(bento1_title, font_cn, 0);
    lv_obj_set_style_text_color(bento1_title, lv_color_hex(0x64748B), 0);
    lv_obj_align(bento1_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_label_bento_temp = lv_label_create(bento_card1);
    lv_label_set_text(s_label_bento_temp, "26.5°C");
    lv_obj_set_style_text_font(s_label_bento_temp, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_label_bento_temp, lv_color_hex(0xFB923C), 0);
    lv_obj_align(s_label_bento_temp, LV_ALIGN_TOP_LEFT, 0, 22);

    s_label_bento_humi = lv_label_create(bento_card1);
    lv_label_set_text(s_label_bento_humi, "湿度: 60.0%");
    lv_obj_set_style_text_font(s_label_bento_humi, font_cn, 0);
    lv_obj_set_style_text_color(s_label_bento_humi, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(s_label_bento_humi, LV_ALIGN_BOTTOM_LEFT, 0, -2);

    // Bento 2 (右侧): 可交互触控智能灯控卡片 (宽111, 高88)
    s_btn_bento_led = lv_button_create(tab1);
    lv_obj_set_size(s_btn_bento_led, 111, 88);
    lv_obj_align(s_btn_bento_led, LV_ALIGN_TOP_RIGHT, 0, 134);
    lv_obj_set_style_bg_color(s_btn_bento_led, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(s_btn_bento_led, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(s_btn_bento_led, 1, 0);
    lv_obj_set_style_radius(s_btn_bento_led, 10, 0);
    lv_obj_set_style_pad_all(s_btn_bento_led, 6, 0);
    lv_obj_add_event_cb(s_btn_bento_led, on_led_btn_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *bento2_title = lv_label_create(s_btn_bento_led);
    lv_label_set_text(bento2_title, "板载智能照明");
    lv_obj_set_style_text_font(bento2_title, font_cn, 0);
    lv_obj_set_style_text_color(bento2_title, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(bento2_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *bento2_icon = lv_label_create(s_btn_bento_led);
    lv_label_set_text(bento2_icon, LV_SYMBOL_POWER);
    lv_obj_set_style_text_font(bento2_icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(bento2_icon, lv_color_hex(0xFACC15), 0);
    lv_obj_align(bento2_icon, LV_ALIGN_TOP_LEFT, 0, 22);

    s_label_bento_led = lv_label_create(s_btn_bento_led);
    lv_label_set_text(s_label_bento_led, "状态: 关闭");
    lv_obj_set_style_text_font(s_label_bento_led, font_cn, 0);
    lv_obj_set_style_text_color(s_label_bento_led, lv_color_hex(0xFACC15), 0);
    lv_obj_align(s_label_bento_led, LV_ALIGN_BOTTOM_LEFT, 0, -2);

    /* =========================================================================
     * ⚡ Tab 2: 赛博空间感知与边缘控制台 (Cyber Sensing & Edge Lab)
     * ========================================================================= */
    lv_obj_t *title2 = lv_label_create(tab2);
    lv_label_set_text(title2, "SENSING & EDGE CONTROL");
    lv_obj_set_style_text_color(title2, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(title2, &lv_font_montserrat_14, 0);
    lv_obj_align(title2, LV_ALIGN_TOP_MID, 0, 2);

    // 1. 双 Bento 科技气象胶囊卡 (宽 228, 高 76, Y: 18)
    // 1.1 左侧温度卡 (宽 110, 高 76)
    lv_obj_t *card_temp = lv_obj_create(tab2);
    lv_obj_set_size(card_temp, 110, 76);
    lv_obj_align(card_temp, LV_ALIGN_TOP_LEFT, 2, 18);
    lv_obj_set_style_bg_color(card_temp, lv_color_hex(0x180F1F), 0);
    lv_obj_set_style_border_color(card_temp, lv_color_hex(0x3B1D2A), 0);
    lv_obj_set_style_border_width(card_temp, 1, 0);
    lv_obj_set_style_radius(card_temp, 10, 0);
    lv_obj_set_style_pad_all(card_temp, 6, 0);
    lv_obj_clear_flag(card_temp, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_t_title = lv_label_create(card_temp);
    lv_label_set_text(lbl_t_title, "室内温度 (NTC)");
    lv_obj_set_style_text_font(lbl_t_title, font_cn, 0);
    lv_obj_set_style_text_color(lbl_t_title, lv_color_hex(0xF87171), 0);
    lv_obj_align(lbl_t_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_label_temp = lv_label_create(card_temp);
    lv_label_set_text(s_label_temp, "25.0 °C");
    lv_obj_set_style_text_font(s_label_temp, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_label_temp, lv_color_hex(0xFB7185), 0);
    lv_obj_align(s_label_temp, LV_ALIGN_TOP_LEFT, 0, 20);

    s_bar_temp = lv_bar_create(card_temp);
    lv_obj_set_size(s_bar_temp, 96, 6);
    lv_obj_align(s_bar_temp, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_bar_set_range(s_bar_temp, 0, 50);
    lv_bar_set_value(s_bar_temp, 25, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar_temp, lv_color_hex(0x27192A), 0);
    lv_obj_set_style_bg_color(s_bar_temp, lv_color_hex(0xF43F5E), LV_PART_INDICATOR);

    // 1.2 右侧湿度卡 (宽 110, 高 76)
    lv_obj_t *card_humi = lv_obj_create(tab2);
    lv_obj_set_size(card_humi, 110, 76);
    lv_obj_align(card_humi, LV_ALIGN_TOP_RIGHT, -2, 18);
    lv_obj_set_style_bg_color(card_humi, lv_color_hex(0x0B192C), 0);
    lv_obj_set_style_border_color(card_humi, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_border_width(card_humi, 1, 0);
    lv_obj_set_style_radius(card_humi, 10, 0);
    lv_obj_set_style_pad_all(card_humi, 6, 0);
    lv_obj_clear_flag(card_humi, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_h_title = lv_label_create(card_humi);
    lv_label_set_text(lbl_h_title, "空气湿度 (DHT)");
    lv_obj_set_style_text_font(lbl_h_title, font_cn, 0);
    lv_obj_set_style_text_color(lbl_h_title, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(lbl_h_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_label_humi = lv_label_create(card_humi);
    lv_label_set_text(s_label_humi, "60.0 %");
    lv_obj_set_style_text_font(s_label_humi, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_label_humi, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(s_label_humi, LV_ALIGN_TOP_LEFT, 0, 20);

    s_bar_humi = lv_bar_create(card_humi);
    lv_obj_set_size(s_bar_humi, 96, 6);
    lv_obj_align(s_bar_humi, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_bar_set_range(s_bar_humi, 0, 100);
    lv_bar_set_value(s_bar_humi, 60, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar_humi, lv_color_hex(0x132742), 0);
    lv_obj_set_style_bg_color(s_bar_humi, lv_color_hex(0x0284C7), LV_PART_INDICATOR);

    // 2. 声呐雷达距离探测卡片 (宽 224, 高 50, Y: 98)
    lv_obj_t *card_dist = lv_obj_create(tab2);
    lv_obj_set_size(card_dist, 224, 50);
    lv_obj_align(card_dist, LV_ALIGN_TOP_MID, 0, 98);
    lv_obj_set_style_bg_color(card_dist, lv_color_hex(0x06281E), 0);
    lv_obj_set_style_border_color(card_dist, lv_color_hex(0x0D523C), 0);
    lv_obj_set_style_border_width(card_dist, 1, 0);
    lv_obj_set_style_radius(card_dist, 10, 0);
    lv_obj_set_style_pad_all(card_dist, 6, 0);
    lv_obj_clear_flag(card_dist, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_d_title = lv_label_create(card_dist);
    lv_label_set_text(lbl_d_title, "超声波雷达探测");
    lv_obj_set_style_text_font(lbl_d_title, font_cn, 0);
    lv_obj_set_style_text_color(lbl_d_title, lv_color_hex(0x34D399), 0);
    lv_obj_align(lbl_d_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_label_dist = lv_label_create(card_dist);
    lv_label_set_text(s_label_dist, "15.0 cm");
    lv_obj_set_style_text_font(s_label_dist, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_label_dist, lv_color_hex(0x10B981), 0);
    lv_obj_align(s_label_dist, LV_ALIGN_TOP_RIGHT, 0, 0);

    s_bar_dist = lv_bar_create(card_dist);
    lv_obj_set_size(s_bar_dist, 210, 8);
    lv_obj_align(s_bar_dist, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_bar_set_range(s_bar_dist, 0, 200);
    lv_bar_set_value(s_bar_dist, 15, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar_dist, lv_color_hex(0x0F3D2E), 0);
    lv_obj_set_style_bg_color(s_bar_dist, lv_color_hex(0x10B981), LV_PART_INDICATOR);

    // 3. 智能照明触控卡片 (宽 224, 高 36, Y: 152)
    s_btn_led = lv_button_create(tab2);
    lv_obj_set_size(s_btn_led, 224, 36);
    lv_obj_align(s_btn_led, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(s_btn_led, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(s_btn_led, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(s_btn_led, 1, 0);
    lv_obj_set_style_radius(s_btn_led, 10, 0);
    lv_obj_clear_flag(s_btn_led, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_btn_led, on_led_btn_clicked, LV_EVENT_CLICKED, NULL);

    s_label_led = lv_label_create(s_btn_led);
    lv_label_set_text(s_label_led, LV_SYMBOL_POWER " 板载智能照明: 关闭");
    lv_obj_set_style_text_font(s_label_led, font_cn, 0);
    lv_obj_center(s_label_led);

    /* =========================================================================
     * 🎨 Tab 3: 电子相册 / 艺术画廊 (全功能 Mini-App)
     * ========================================================================= */
    ui_photo_album_init(tab3);

    /* =========================================================================
     * 📖 Tab 4: TF 卡电子小说阅读器 (全功能 Mini-App)
     * ========================================================================= */
    ui_novel_reader_init(tab4);

    /* =========================================================================
     * ⚙️ Tab 5: 系统架构与节点运维中枢 (System Architecture Matrix)
     * ========================================================================= */
    lv_obj_t *title5 = lv_label_create(tab5);
    lv_label_set_text(title5, "SYSTEM & TELEMETRY");
    lv_obj_set_style_text_color(title5, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(title5, &lv_font_montserrat_14, 0);
    lv_obj_align(title5, LV_ALIGN_TOP_MID, 0, 2);

    // 1. 节点网络与心跳卡 (宽 228, 高 48, Y: 18)
    lv_obj_t *card_net = lv_obj_create(tab5);
    lv_obj_set_size(card_net, 228, 48);
    lv_obj_align(card_net, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_set_style_bg_color(card_net, lv_color_hex(0x0B1329), 0);
    lv_obj_set_style_border_color(card_net, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(card_net, 1, 0);
    lv_obj_set_style_radius(card_net, 8, 0);
    lv_obj_set_style_pad_all(card_net, 5, 0);
    lv_obj_clear_flag(card_net, LV_OBJ_FLAG_SCROLLABLE);

    s_label_bento_ip = lv_label_create(card_net);
    lv_obj_set_width(s_label_bento_ip, 216);
    lv_label_set_long_mode(s_label_bento_ip, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_label_bento_ip, "网络: CalvinHome (192.168.4.1)");
    lv_obj_set_style_text_font(s_label_bento_ip, font_cn, 0);
    lv_obj_set_style_text_color(s_label_bento_ip, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(s_label_bento_ip, LV_ALIGN_TOP_LEFT, 2, 0);

    s_label_sys_uptime = lv_label_create(card_net);
    lv_obj_set_width(s_label_sys_uptime, 216);
    lv_label_set_long_mode(s_label_sys_uptime, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_label_sys_uptime, "心跳: 00:00:00 (在线)");
    lv_obj_set_style_text_font(s_label_sys_uptime, font_cn, 0);
    lv_obj_set_style_text_color(s_label_sys_uptime, lv_color_hex(0x10B981), 0);
    lv_obj_align(s_label_sys_uptime, LV_ALIGN_TOP_LEFT, 2, 18);

    // 2. 内存架构双 Bento 胶囊 (宽 228, 高 50, Y: 70)
    // 2.1 SRAM 胶囊 (宽 110, 高 50)
    lv_obj_t *box_sram = lv_obj_create(tab5);
    lv_obj_set_size(box_sram, 110, 50);
    lv_obj_align(box_sram, LV_ALIGN_TOP_LEFT, 2, 70);
    lv_obj_set_style_bg_color(box_sram, lv_color_hex(0x06281E), 0);
    lv_obj_set_style_border_color(box_sram, lv_color_hex(0x0D523C), 0);
    lv_obj_set_style_border_width(box_sram, 1, 0);
    lv_obj_set_style_radius(box_sram, 8, 0);
    lv_obj_set_style_pad_all(box_sram, 5, 0);
    lv_obj_clear_flag(box_sram, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_sr_title = lv_label_create(box_sram);
    lv_label_set_text(lbl_sr_title, "内部 SRAM 缓存");
    lv_obj_set_style_text_font(lbl_sr_title, font_cn, 0);
    lv_obj_set_style_text_color(lbl_sr_title, lv_color_hex(0x34D399), 0);
    lv_obj_align(lbl_sr_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_label_sys_heap = lv_label_create(box_sram);
    lv_obj_set_width(s_label_sys_heap, 100);
    lv_label_set_long_mode(s_label_sys_heap, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_label_sys_heap, "186 KB 剩余");
    lv_obj_set_style_text_font(s_label_sys_heap, font_cn, 0);
    lv_obj_set_style_text_color(s_label_sys_heap, lv_color_hex(0x10B981), 0);
    lv_obj_align(s_label_sys_heap, LV_ALIGN_TOP_LEFT, 0, 18);

    // 2.2 PSRAM 胶囊 (宽 110, 高 50)
    lv_obj_t *box_psram = lv_obj_create(tab5);
    lv_obj_set_size(box_psram, 110, 50);
    lv_obj_align(box_psram, LV_ALIGN_TOP_RIGHT, -2, 70);
    lv_obj_set_style_bg_color(box_psram, lv_color_hex(0x1A0F2E), 0);
    lv_obj_set_style_border_color(box_psram, lv_color_hex(0x3D1A5E), 0);
    lv_obj_set_style_border_width(box_psram, 1, 0);
    lv_obj_set_style_radius(box_psram, 8, 0);
    lv_obj_set_style_pad_all(box_psram, 5, 0);
    lv_obj_clear_flag(box_psram, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_ps_title = lv_label_create(box_psram);
    lv_label_set_text(lbl_ps_title, "外部 PSRAM 显存");
    lv_obj_set_style_text_font(lbl_ps_title, font_cn, 0);
    lv_obj_set_style_text_color(lbl_ps_title, lv_color_hex(0xC084FC), 0);
    lv_obj_align(lbl_ps_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_label_sys_psram = lv_label_create(box_psram);
    lv_obj_set_width(s_label_sys_psram, 100);
    lv_label_set_long_mode(s_label_sys_psram, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_label_sys_psram, "1.98 MB 剩余");
    lv_obj_set_style_text_font(s_label_sys_psram, font_cn, 0);
    lv_obj_set_style_text_color(s_label_sys_psram, lv_color_hex(0xA78BFA), 0);
    lv_obj_align(s_label_sys_psram, LV_ALIGN_TOP_LEFT, 0, 18);

    // 3. 硬件主控与存储规格卡片 (宽 228, 高 50, Y: 124)
    lv_obj_t *card_hw = lv_obj_create(tab5);
    lv_obj_set_size(card_hw, 228, 50);
    lv_obj_align(card_hw, LV_ALIGN_TOP_MID, 0, 124);
    lv_obj_set_style_bg_color(card_hw, lv_color_hex(0x131C38), 0);
    lv_obj_set_style_border_color(card_hw, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(card_hw, 1, 0);
    lv_obj_set_style_radius(card_hw, 8, 0);
    lv_obj_set_style_pad_all(card_hw, 5, 0);
    lv_obj_clear_flag(card_hw, LV_OBJ_FLAG_SCROLLABLE);

    s_label_sys_chip = lv_label_create(card_hw);
    lv_obj_set_width(s_label_sys_chip, 216);
    lv_label_set_long_mode(s_label_sys_chip, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_label_sys_chip, "存储: TF卡已挂载 (8GB)");
    lv_obj_set_style_text_font(s_label_sys_chip, font_cn, 0);
    lv_obj_set_style_text_color(s_label_sys_chip, lv_color_hex(0xFBBF24), 0);
    lv_obj_align(s_label_sys_chip, LV_ALIGN_TOP_LEFT, 2, 0);

    s_label_ota = lv_label_create(card_hw);
    lv_obj_set_width(s_label_ota, 216);
    lv_label_set_long_mode(s_label_ota, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_label_ota, "主控: ESP32 · OTA_0 运行");
    lv_obj_set_style_text_font(s_label_ota, font_cn, 0);
    lv_obj_set_style_text_color(s_label_ota, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(s_label_ota, LV_ALIGN_TOP_LEFT, 2, 18);

    // 4. 底部双运维动作栏 (重置 Wi-Fi / 重启系统, 高 34, Y: 180)
    lv_obj_t *btn_reset_wifi = lv_button_create(tab5);
    lv_obj_set_size(btn_reset_wifi, 110, 34);
    lv_obj_align(btn_reset_wifi, LV_ALIGN_TOP_LEFT, 2, 180);
    lv_obj_set_style_bg_color(btn_reset_wifi, lv_color_hex(0x991B1B), 0);
    lv_obj_set_style_radius(btn_reset_wifi, 6, 0);
    lv_obj_clear_flag(btn_reset_wifi, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_reset_wifi, on_reset_wifi_btn_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_rw = lv_label_create(btn_reset_wifi);
    lv_label_set_text(lbl_rw, LV_SYMBOL_REFRESH " 配网重置");
    lv_obj_set_style_text_font(lbl_rw, font_cn, 0);
    lv_obj_center(lbl_rw);

    lv_obj_t *btn_reboot = lv_button_create(tab5);
    lv_obj_set_size(btn_reboot, 110, 34);
    lv_obj_align(btn_reboot, LV_ALIGN_TOP_RIGHT, -2, 180);
    lv_obj_set_style_bg_color(btn_reboot, lv_color_hex(0x0369A1), 0);
    lv_obj_set_style_radius(btn_reboot, 6, 0);
    lv_obj_clear_flag(btn_reboot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_reboot, on_reboot_system_btn_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_rb = lv_label_create(btn_reboot);
    lv_label_set_text(lbl_rb, LV_SYMBOL_POWER " 重启系统");
    lv_obj_set_style_text_font(lbl_rb, font_cn, 0);
    lv_obj_center(lbl_rb);

    bsp_lvgl_port_unlock();
    ESP_LOGI(TAG, "🎨 [UI] LVGL v9 5-App 触控桌面系统初始化成功 (底部 Dock 布局)！");

    /* =========================================================================
     * 📈 Tab 6: 传感器 24H 趋势折线图 (Analytics)
     * ========================================================================= */
    bsp_lvgl_port_lock(0);
    ui_analytics_init(tab6);
    bsp_lvgl_port_unlock();

    /* =========================================================================
     * ⏱️ Tab 7: 专注番茄钟 / 大号数字时钟
     * ========================================================================= */
    bsp_lvgl_port_lock(0);
    ui_pomodoro_init(tab7);
    bsp_lvgl_port_unlock();

    /* =========================================================================
     * 🎮 Tab 8: 2048 触控数字游戏
     * ========================================================================= */
    bsp_lvgl_port_lock(0);
    ui_game_2048_init(tab8);
    bsp_lvgl_port_unlock();

    /* =========================================================================
     * 🎛️ 全局下拉控制中心（挂载到 scr 最顶层）
     *    注：控制中心必须最后创建，以覆盖在所有 Tab 上层
     * ========================================================================= */
    bsp_lvgl_port_lock(0);
    ui_control_center_init(scr);
    bsp_lvgl_port_unlock();

    ESP_LOGI(TAG, "🚀 [UI] 全功能 8-Tab 系统 + 控制中心初始化完毕！");
}

void ui_hub_update_sensor_data(const bsp_sensor_data_t *data)
{
    if (!data) return;
    bsp_lvgl_port_lock(0);

    char buf[64];

    // 1. 驱动 Tab 1 Cyber Bento 微卡片组件
    if (s_label_bento_temp) {
        snprintf(buf, sizeof(buf), "%.1f°C", data->ntc_temperature);
        lv_label_set_text(s_label_bento_temp, buf);
    }
    if (s_label_bento_humi) {
        snprintf(buf, sizeof(buf), "%.1f%%", data->dht_humidity);
        lv_label_set_text(s_label_bento_humi, buf);
    }

    // 2. 驱动 Tab 2 详细传感器看板
    if (s_label_temp) {
        snprintf(buf, sizeof(buf), "%.1f °C", data->ntc_temperature);
        lv_label_set_text(s_label_temp, buf);
    }
    // 推送数据到折线图 (Tab 6 Analytics)
    ui_analytics_push_data(data->ntc_temperature, data->dht_humidity);
    if (s_bar_temp) {
        lv_bar_set_value(s_bar_temp, (int)data->ntc_temperature, LV_ANIM_ON);
    }

    if (s_label_humi) {
        snprintf(buf, sizeof(buf), "%.1f %%", data->dht_humidity);
        lv_label_set_text(s_label_humi, buf);
    }
    if (s_bar_humi) {
        lv_bar_set_value(s_bar_humi, (int)data->dht_humidity, LV_ANIM_ON);
    }

    if (s_label_dist) {
        snprintf(buf, sizeof(buf), "%.1f cm", data->ultrasonic_dist_cm);
        lv_label_set_text(s_label_dist, buf);
    }
    if (s_bar_dist) {
        lv_bar_set_value(s_bar_dist, (int)data->ultrasonic_dist_cm, LV_ANIM_ON);
    }

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
            lv_obj_set_style_text_color(s_label_top_sd, lv_color_hex(0x10B981), 0);
        } else {
            lv_label_set_text(s_label_top_sd, LV_SYMBOL_SD_CARD " 无SD卡");
            lv_obj_set_style_text_color(s_label_top_sd, lv_color_hex(0x64748B), 0);
        }
    }

    if (ip_str) {
        if (net_manager_is_provisioning()) {
            if (s_label_wifi_icon) {
                lv_label_set_text(s_label_wifi_icon, LV_SYMBOL_WIFI);
                lv_obj_set_style_text_color(s_label_wifi_icon, lv_color_hex(0xF59E0B), 0);
            }
            if (s_label_top_net) {
                lv_label_set_text(s_label_top_net, "AP配网中");
                lv_obj_set_style_text_color(s_label_top_net, lv_color_hex(0xF59E0B), 0);
            }
            if (s_label_bento_ip) {
                lv_label_set_text(s_label_bento_ip, "网络: AP热点 (192.168.4.1)");
            }
        } else if (net_manager_is_wifi_connected()) {
            if (s_label_wifi_icon) {
                lv_label_set_text(s_label_wifi_icon, LV_SYMBOL_WIFI);
                lv_obj_set_style_text_color(s_label_wifi_icon, lv_color_hex(0x10B981), 0);
            }
            if (s_label_top_net) {
                lv_label_set_text(s_label_top_net, "WiFi在线");
                lv_obj_set_style_text_color(s_label_top_net, lv_color_hex(0x10B981), 0);
            }
            if (s_label_bento_ip) {
                char sbuf[64];
                snprintf(sbuf, sizeof(sbuf), "网络: 已连接 (%s)", ip_str);
                lv_label_set_text(s_label_bento_ip, sbuf);
            }
        } else {
            if (s_label_wifi_icon) {
                lv_label_set_text(s_label_wifi_icon, LV_SYMBOL_WARNING);
                lv_obj_set_style_text_color(s_label_wifi_icon, lv_color_hex(0x64748B), 0);
            }
            if (s_label_top_net) {
                lv_label_set_text(s_label_top_net, "连接中...");
                lv_obj_set_style_text_color(s_label_top_net, lv_color_hex(0x64748B), 0);
            }
            if (s_label_bento_ip) {
                lv_label_set_text(s_label_bento_ip, "网络: 正在连接 WiFi...");
            }
        }
    }

    // 2. 驱动 Tab 5 详细运维系统卡片
    if (uptime_str && s_label_sys_uptime) {
        char buf[64];
        snprintf(buf, sizeof(buf), "心跳: %s (在线)", uptime_str);
        lv_label_set_text(s_label_sys_uptime, buf);
    }

    if (bsp_sdcard_is_mounted()) {
        if (s_label_top_sd) {
            lv_label_set_text(s_label_top_sd, LV_SYMBOL_SD_CARD " SD在线");
            lv_obj_set_style_text_color(s_label_top_sd, lv_color_hex(0x38BDF8), 0);
        }
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
        if (s_label_top_sd) {
            lv_label_set_text(s_label_top_sd, LV_SYMBOL_SD_CARD " 无SD卡");
            lv_obj_set_style_text_color(s_label_top_sd, lv_color_hex(0x64748B), 0);
        }
        if (s_label_sys_chip) {
            lv_label_set_text(s_label_sys_chip, "存储: 未插 TF 卡 (Flash)");
        }
    }

    if (s_label_sys_heap) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%lu KB 剩余", (unsigned long)(heap / 1024));
        lv_label_set_text(s_label_sys_heap, buf);
    }
    if (s_label_sys_psram) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.2f MB 剩余", (float)psram / (1024.0f * 1024.0f));
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

