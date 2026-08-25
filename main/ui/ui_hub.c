#include "ui_hub.h"
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "bsp_lvgl_port.h"
#include "bsp_led.h"
#include "file_reader.h"
#include "sys_font_manager.h"
#include "ui_novel_reader.h"
#include "net_manager.h"
#include "esp_log.h"

static const char *TAG = "UI_HUB";

static lv_obj_t *s_tabview = NULL;

// Tab 1: 气象时钟组件
static lv_obj_t *s_label_location = NULL;
static lv_obj_t *s_label_wifi_icon = NULL;
static lv_obj_t *s_label_clock = NULL;
static lv_obj_t *s_label_date = NULL;
static lv_obj_t *s_label_weather_main = NULL;
static lv_obj_t *s_label_weather_sub = NULL;
static lv_obj_t *s_label_ip = NULL;

// Tab 2: 传感器 IoT 仪表组件
static lv_obj_t *s_bar_temp = NULL;
static lv_obj_t *s_label_temp = NULL;
static lv_obj_t *s_bar_humi = NULL;
static lv_obj_t *s_label_humi = NULL;
static lv_obj_t *s_bar_dist = NULL;
static lv_obj_t *s_label_dist = NULL;
static lv_obj_t *s_btn_led = NULL;
static lv_obj_t *s_label_led = NULL;

// Tab 3: 电子相册组件
static lv_obj_t *s_album_label = NULL;
static int s_current_photo_idx = 0;

// Tab 5: 系统设置组件
static lv_obj_t *s_label_sys_uptime = NULL;
static lv_obj_t *s_label_sys_chip = NULL;
static lv_obj_t *s_label_sys_heap = NULL;
static lv_obj_t *s_label_sys_psram = NULL;
static lv_obj_t *s_label_ota = NULL;

/* 按钮事件：LED 翻转 */
static void on_led_btn_clicked(lv_event_t *e)
{
    bsp_led_toggle();
    bool state = bsp_led_get_state();
    if (state) {
        lv_label_set_text(s_label_led, LV_SYMBOL_POWER " LED: ON");
        lv_obj_set_style_bg_color(s_btn_led, lv_color_hex(0x10B981), 0);
    } else {
        lv_label_set_text(s_label_led, LV_SYMBOL_POWER " LED: OFF");
        lv_obj_set_style_bg_color(s_btn_led, lv_color_hex(0x475569), 0);
    }
}

/* 相册翻页事件 */
static void on_album_next_clicked(lv_event_t *e)
{
    int count = file_reader_get_photo_count();
    s_current_photo_idx = (s_current_photo_idx + 1) % count;
    char path[280];
    file_reader_get_photo_path(s_current_photo_idx, path, sizeof(path));
    char buf[320];
    snprintf(buf, sizeof(buf), "[Photo %d/%d]\n%s", s_current_photo_idx + 1, count, path);
    lv_label_set_text(s_album_label, buf);
}

/* 重置 Wi-Fi 事件 */
static void on_reset_wifi_btn_clicked(lv_event_t *e)
{
    ESP_LOGW(TAG, "🔘 用户点击屏幕【重置 Wi-Fi】按钮");
    net_manager_reset_credentials();
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

    // 5 个 Tab 按钮采用标准高清图标
    lv_obj_t *tab1 = lv_tabview_add_tab(s_tabview, LV_SYMBOL_HOME);
    lv_obj_t *tab2 = lv_tabview_add_tab(s_tabview, LV_SYMBOL_CHARGE);
    lv_obj_t *tab3 = lv_tabview_add_tab(s_tabview, LV_SYMBOL_IMAGE);
    lv_obj_t *tab4 = lv_tabview_add_tab(s_tabview, LV_SYMBOL_FILE);
    lv_obj_t *tab5 = lv_tabview_add_tab(s_tabview, LV_SYMBOL_SETTINGS);

    // 消除 Tab 内部默认冗余边距
    lv_obj_t *tabs[] = {tab1, tab2, tab3, tab4, tab5};
    for (int i = 0; i < 5; i++) {
        lv_obj_set_style_pad_all(tabs[i], 6, 0);
        lv_obj_set_style_bg_color(tabs[i], lv_color_hex(0x0A0F1D), 0);
    }

    /* =========================================================================
     * 🌤️ Tab 1: 桌面智慧气象时钟看板
     * ========================================================================= */
    s_label_location = lv_label_create(tab1);
    lv_label_set_text(s_label_location, "深圳市 · 晴朗");
    lv_obj_set_style_text_color(s_label_location, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_text_font(s_label_location, font_cn, 0);
    lv_obj_align(s_label_location, LV_ALIGN_TOP_LEFT, 4, 4);

    s_label_wifi_icon = lv_label_create(tab1);
    lv_label_set_text(s_label_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(s_label_wifi_icon, lv_color_hex(0x64748B), 0);
    lv_obj_align(s_label_wifi_icon, LV_ALIGN_TOP_RIGHT, -4, 4);

    // 醒目大字号数字时钟 (HH:MM:SS)
    s_label_clock = lv_label_create(tab1);
    lv_label_set_text(s_label_clock, "16:00:00");
    lv_obj_set_style_text_font(s_label_clock, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(s_label_clock, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(s_label_clock, LV_ALIGN_TOP_MID, 0, 24);

    // 精确公历年月日与星期
    s_label_date = lv_label_create(tab1);
    lv_label_set_text(s_label_date, "2026年08月25日 星期二");
    lv_obj_set_style_text_font(s_label_date, font_cn, 0);
    lv_obj_set_style_text_color(s_label_date, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(s_label_date, LV_ALIGN_TOP_MID, 0, 64);

    // 磨砂气象与环境卡片
    lv_obj_t *weather_card = lv_obj_create(tab1);
    lv_obj_set_size(weather_card, 222, 78);
    lv_obj_align(weather_card, LV_ALIGN_CENTER, 0, 22);
    lv_obj_set_style_bg_color(weather_card, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(weather_card, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(weather_card, 12, 0);
    lv_obj_set_style_pad_all(weather_card, 6, 0);

    s_label_weather_main = lv_label_create(weather_card);
    lv_label_set_text(s_label_weather_main, "晴朗舒适 · 适宜阅读");
    lv_obj_set_style_text_font(s_label_weather_main, font_cn, 0);
    lv_obj_set_style_text_color(s_label_weather_main, lv_color_hex(0xFACC15), 0);
    lv_obj_align(s_label_weather_main, LV_ALIGN_TOP_MID, 0, 4);

    s_label_weather_sub = lv_label_create(weather_card);
    lv_label_set_text(s_label_weather_sub, "26.5°C  60.0%  空气优");
    lv_obj_set_style_text_font(s_label_weather_sub, font_cn, 0);
    lv_obj_set_style_text_color(s_label_weather_sub, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(s_label_weather_sub, LV_ALIGN_BOTTOM_MID, 0, -6);

    s_label_ip = lv_label_create(tab1);
    lv_label_set_text(s_label_ip, "WiFi: 正在连接...");
    lv_obj_set_style_text_font(s_label_ip, font_cn, 0);
    lv_obj_set_style_text_color(s_label_ip, lv_color_hex(0x64748B), 0);
    lv_obj_align(s_label_ip, LV_ALIGN_BOTTOM_MID, 0, -4);

    /* =========================================================================
     * 🎛️ Tab 2: 传感器与 IoT 中控
     * ========================================================================= */
    lv_obj_t *title2 = lv_label_create(tab2);
    lv_label_set_text(title2, "ENVIRONMENT & IOT");
    lv_obj_set_style_text_color(title2, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(title2, &lv_font_montserrat_14, 0);
    lv_obj_align(title2, LV_ALIGN_TOP_MID, 0, 2);

    s_label_temp = lv_label_create(tab2);
    lv_label_set_text(s_label_temp, "NTC Temp: 25.0 °C");
    lv_obj_set_style_text_color(s_label_temp, lv_color_hex(0xF87171), 0);
    lv_obj_set_style_text_font(s_label_temp, &lv_font_montserrat_14, 0);
    lv_obj_align(s_label_temp, LV_ALIGN_TOP_LEFT, 4, 22);

    s_bar_temp = lv_bar_create(tab2);
    lv_obj_set_size(s_bar_temp, 218, 7);
    lv_obj_align(s_bar_temp, LV_ALIGN_TOP_LEFT, 4, 38);
    lv_bar_set_range(s_bar_temp, 0, 50);
    lv_bar_set_value(s_bar_temp, 25, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar_temp, lv_color_hex(0xEF4444), LV_PART_INDICATOR);

    s_label_humi = lv_label_create(tab2);
    lv_label_set_text(s_label_humi, "DHT11 Humi: 60.0 %");
    lv_obj_set_style_text_color(s_label_humi, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_text_font(s_label_humi, &lv_font_montserrat_14, 0);
    lv_obj_align(s_label_humi, LV_ALIGN_TOP_LEFT, 4, 52);

    s_bar_humi = lv_bar_create(tab2);
    lv_obj_set_size(s_bar_humi, 218, 7);
    lv_obj_align(s_bar_humi, LV_ALIGN_TOP_LEFT, 4, 68);
    lv_bar_set_range(s_bar_humi, 0, 100);
    lv_bar_set_value(s_bar_humi, 60, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar_humi, lv_color_hex(0x0284C7), LV_PART_INDICATOR);

    s_label_dist = lv_label_create(tab2);
    lv_label_set_text(s_label_dist, "Distance: 15.0 cm");
    lv_obj_set_style_text_color(s_label_dist, lv_color_hex(0x34D399), 0);
    lv_obj_set_style_text_font(s_label_dist, &lv_font_montserrat_14, 0);
    lv_obj_align(s_label_dist, LV_ALIGN_TOP_LEFT, 4, 82);

    s_bar_dist = lv_bar_create(tab2);
    lv_obj_set_size(s_bar_dist, 218, 7);
    lv_obj_align(s_bar_dist, LV_ALIGN_TOP_LEFT, 4, 98);
    lv_bar_set_range(s_bar_dist, 0, 200);
    lv_bar_set_value(s_bar_dist, 15, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar_dist, lv_color_hex(0x10B981), LV_PART_INDICATOR);

    s_btn_led = lv_button_create(tab2);
    lv_obj_set_size(s_btn_led, 218, 36);
    lv_obj_align(s_btn_led, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(s_btn_led, lv_color_hex(0x475569), 0);
    lv_obj_set_style_radius(s_btn_led, 10, 0);
    lv_obj_add_event_cb(s_btn_led, on_led_btn_clicked, LV_EVENT_CLICKED, NULL);

    s_label_led = lv_label_create(s_btn_led);
    lv_label_set_text(s_label_led, LV_SYMBOL_POWER " LED: OFF");
    lv_obj_set_style_text_font(s_label_led, &lv_font_montserrat_14, 0);
    lv_obj_center(s_label_led);

    /* =========================================================================
     * 🖼️ Tab 3: 电子相册
     * ========================================================================= */
    lv_obj_t *title3 = lv_label_create(tab3);
    lv_label_set_text(title3, "DIGITAL PHOTO FRAME");
    lv_obj_set_style_text_color(title3, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(title3, &lv_font_montserrat_14, 0);
    lv_obj_align(title3, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t *photo_box = lv_obj_create(tab3);
    lv_obj_set_size(photo_box, 218, 140);
    lv_obj_align(photo_box, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_color(photo_box, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(photo_box, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(photo_box, 12, 0);

    s_album_label = lv_label_create(photo_box);
    lv_label_set_text(s_album_label, "[TF Card Photos]\nNo photo detected in\n/sdcard/photos/\nClick below to demo.");
    lv_obj_set_style_text_color(s_album_label, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(s_album_label, &lv_font_montserrat_14, 0);
    lv_obj_center(s_album_label);

    lv_obj_t *btn_next_photo = lv_button_create(tab3);
    lv_obj_set_size(btn_next_photo, 180, 36);
    lv_obj_align(btn_next_photo, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(btn_next_photo, lv_color_hex(0x2563EB), 0);
    lv_obj_set_style_radius(btn_next_photo, 10, 0);
    lv_obj_add_event_cb(btn_next_photo, on_album_next_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_np = lv_label_create(btn_next_photo);
    lv_label_set_text(lbl_np, LV_SYMBOL_NEXT " Next Photo");
    lv_obj_set_style_text_font(lbl_np, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_np);

    /* =========================================================================
     * 📖 Tab 4: TF 卡电子小说阅读器 (全功能 Mini-App)
     * ========================================================================= */
    ui_novel_reader_init(tab4);

    /* =========================================================================
     * ⚙️ Tab 5: 系统设置与运行状态
     * ========================================================================= */
    lv_obj_t *title5 = lv_label_create(tab5);
    lv_label_set_text(title5, "SYSTEM & SETTINGS");
    lv_obj_set_style_text_color(title5, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(title5, &lv_font_montserrat_14, 0);
    lv_obj_align(title5, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t *sys_card = lv_obj_create(tab5);
    lv_obj_set_size(sys_card, 222, 136);
    lv_obj_align(sys_card, LV_ALIGN_CENTER, 0, -8);
    lv_obj_set_style_bg_color(sys_card, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(sys_card, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(sys_card, 12, 0);
    lv_obj_set_style_pad_all(sys_card, 8, 0);

    s_label_sys_uptime = lv_label_create(sys_card);
    lv_label_set_text(s_label_sys_uptime, "运行时间: 00:00:00 (0s)");
    lv_obj_set_style_text_font(s_label_sys_uptime, font_cn, 0);
    lv_obj_set_style_text_color(s_label_sys_uptime, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(s_label_sys_uptime, LV_ALIGN_TOP_LEFT, 2, 2);

    s_label_sys_chip = lv_label_create(sys_card);
    lv_label_set_text(s_label_sys_chip, "主控: ESP32 240MHz 8MB");
    lv_obj_set_style_text_font(s_label_sys_chip, font_cn, 0);
    lv_obj_set_style_text_color(s_label_sys_chip, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(s_label_sys_chip, LV_ALIGN_TOP_LEFT, 2, 24);

    s_label_sys_heap = lv_label_create(sys_card);
    lv_label_set_text(s_label_sys_heap, "内存: Free Heap 180 KB");
    lv_obj_set_style_text_font(s_label_sys_heap, font_cn, 0);
    lv_obj_set_style_text_color(s_label_sys_heap, lv_color_hex(0x10B981), 0);
    lv_obj_align(s_label_sys_heap, LV_ALIGN_TOP_LEFT, 2, 46);

    s_label_sys_psram = lv_label_create(sys_card);
    lv_label_set_text(s_label_sys_psram, "显存: PSRAM 1.8 MB Free");
    lv_obj_set_style_text_font(s_label_sys_psram, font_cn, 0);
    lv_obj_set_style_text_color(s_label_sys_psram, lv_color_hex(0xA78BFA), 0);
    lv_obj_align(s_label_sys_psram, LV_ALIGN_TOP_LEFT, 2, 68);

    s_label_ota = lv_label_create(sys_card);
    lv_label_set_text(s_label_ota, "固件: v2.1.0-Hub 就绪");
    lv_obj_set_style_text_font(s_label_ota, font_cn, 0);
    lv_obj_set_style_text_color(s_label_ota, lv_color_hex(0xFBBF24), 0);
    lv_obj_align(s_label_ota, LV_ALIGN_TOP_LEFT, 2, 90);

    // 重置 Wi-Fi 重新配网按钮
    lv_obj_t *btn_reset_wifi = lv_button_create(tab5);
    lv_obj_set_size(btn_reset_wifi, 222, 34);
    lv_obj_align(btn_reset_wifi, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(btn_reset_wifi, lv_color_hex(0xDC2626), 0);
    lv_obj_set_style_radius(btn_reset_wifi, 8, 0);
    lv_obj_add_event_cb(btn_reset_wifi, on_reset_wifi_btn_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_rw = lv_label_create(btn_reset_wifi);
    lv_label_set_text(lbl_rw, LV_SYMBOL_REFRESH " 重置 Wi-Fi 重新配网");
    lv_obj_set_style_text_font(lbl_rw, font_cn, 0);
    lv_obj_center(lbl_rw);

    bsp_lvgl_port_unlock();
    ESP_LOGI(TAG, "🎨 [UI] LVGL v9 5-App 触控桌面系统初始化成功 (底部 Dock 布局)！");
}

void ui_hub_update_sensor_data(const bsp_sensor_data_t *data)
{
    if (!data) return;
    bsp_lvgl_port_lock(0);

    char buf[64];
    snprintf(buf, sizeof(buf), "NTC Temp: %.1f °C", data->ntc_temperature);
    lv_label_set_text(s_label_temp, buf);
    lv_bar_set_value(s_bar_temp, (int)data->ntc_temperature, LV_ANIM_ON);

    snprintf(buf, sizeof(buf), "DHT11 Humi: %.1f %%", data->dht_humidity);
    lv_label_set_text(s_label_humi, buf);
    lv_bar_set_value(s_bar_humi, (int)data->dht_humidity, LV_ANIM_ON);

    snprintf(buf, sizeof(buf), "Distance: %.1f cm", data->ultrasonic_dist_cm);
    lv_label_set_text(s_label_dist, buf);
    lv_bar_set_value(s_bar_dist, (int)data->ultrasonic_dist_cm, LV_ANIM_ON);

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
    if (location && s_label_location) {
        lv_label_set_text(s_label_location, location);
    }
    if (weather_desc && s_label_weather_main) {
        lv_label_set_text(s_label_weather_main, weather_desc);
    }
    if (s_label_weather_sub) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.1f°C  %.1f%%  空气优", temp, humi);
        lv_label_set_text(s_label_weather_sub, buf);
    }
    bsp_lvgl_port_unlock();
}

void ui_hub_update_system_status(const char *uptime_str, const char *ip_str, uint32_t heap, uint32_t psram)
{
    bsp_lvgl_port_lock(0);
    if (uptime_str && s_label_sys_uptime) {
        char buf[64];
        snprintf(buf, sizeof(buf), "运行时间: %s", uptime_str);
        lv_label_set_text(s_label_sys_uptime, buf);
    }
    if (ip_str && s_label_ip) {
        char buf[64];
        if (net_manager_is_provisioning()) {
            snprintf(buf, sizeof(buf), "热点配网: %s", ip_str);
            lv_label_set_text(s_label_ip, buf);
            if (s_label_wifi_icon) lv_obj_set_style_text_color(s_label_wifi_icon, lv_color_hex(0xF59E0B), 0);
            if (s_label_location) lv_label_set_text(s_label_location, "配网向导 · 请连热点");
            if (s_label_weather_main) lv_label_set_text(s_label_weather_main, "手机连热点 192.168.4.1 配网");
            if (s_label_weather_sub) lv_label_set_text(s_label_weather_sub, "扫码或浏览器访问即可设置");
        } else if (net_manager_is_wifi_connected()) {
            snprintf(buf, sizeof(buf), "WiFi: %s", ip_str);
            lv_label_set_text(s_label_ip, buf);
            if (s_label_wifi_icon) lv_obj_set_style_text_color(s_label_wifi_icon, lv_color_hex(0x10B981), 0);
        } else {
            lv_label_set_text(s_label_ip, "WiFi: 正在连接路由器...");
            if (s_label_wifi_icon) lv_obj_set_style_text_color(s_label_wifi_icon, lv_color_hex(0x64748B), 0);
        }
    }
    if (s_label_sys_heap) {
        char buf[64];
        snprintf(buf, sizeof(buf), "内存: Free Heap %lu KB", (unsigned long)(heap / 1024));
        lv_label_set_text(s_label_sys_heap, buf);
    }
    if (s_label_sys_psram) {
        char buf[64];
        snprintf(buf, sizeof(buf), "显存: PSRAM %.1f MB Free", (float)psram / (1024.0f * 1024.0f));
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
