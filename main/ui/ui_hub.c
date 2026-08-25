#include "ui_hub.h"
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "bsp_lvgl_port.h"
#include "bsp_led.h"
#include "file_reader.h"
#include "ui_fonts.h"
#include "ui_icons.h"
#include "esp_log.h"

static const char *TAG = "UI_HUB";

static lv_obj_t *s_tabview = NULL;

// Tab 1: 气象时钟组件
static lv_obj_t *s_label_clock = NULL;
static lv_obj_t *s_label_date = NULL;
static lv_obj_t *s_label_weather = NULL;
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

// Tab 4: 电子小说组件
static lv_obj_t *s_novel_text = NULL;
static lv_obj_t *s_novel_page_label = NULL;
static int s_current_novel_page = 0;
static int s_total_novel_pages = 1;

// Tab 5: 系统设置组件
static lv_obj_t *s_label_sys_heap = NULL;
static lv_obj_t *s_label_sys_psram = NULL;
static lv_obj_t *s_bar_ota = NULL;
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
    char path[64];
    file_reader_get_photo_path(s_current_photo_idx, path, sizeof(path));
    char buf[128];
    snprintf(buf, sizeof(buf), "[Photo %d/%d]\n%s", s_current_photo_idx + 1, count, path);
    lv_label_set_text(s_album_label, buf);
}

/* 小说翻页事件 */
static void on_novel_prev_clicked(lv_event_t *e)
{
    if (s_current_novel_page > 0) {
        s_current_novel_page--;
        char text_buf[256];
        file_reader_load_novel_page(s_current_novel_page, text_buf, sizeof(text_buf), &s_total_novel_pages);
        lv_label_set_text(s_novel_text, text_buf);
        char page_buf[32];
        snprintf(page_buf, sizeof(page_buf), "Page %d / %d", s_current_novel_page + 1, s_total_novel_pages);
        lv_label_set_text(s_novel_page_label, page_buf);
    }
}

static void on_novel_next_clicked(lv_event_t *e)
{
    if (s_current_novel_page < s_total_novel_pages - 1) {
        s_current_novel_page++;
        char text_buf[256];
        file_reader_load_novel_page(s_current_novel_page, text_buf, sizeof(text_buf), &s_total_novel_pages);
        lv_label_set_text(s_novel_text, text_buf);
        char page_buf[32];
        snprintf(page_buf, sizeof(page_buf), "Page %d / %d", s_current_novel_page + 1, s_total_novel_pages);
        lv_label_set_text(s_novel_page_label, page_buf);
    }
}

void ui_hub_init(void)
{
    bsp_lvgl_port_lock(0);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0A0F1D), 0); // 宇宙黑背景

    // 1. 创建多应用滑动 TabView
    s_tabview = lv_tabview_create(scr);
    lv_obj_set_size(s_tabview, 240, 280);
    lv_obj_set_style_bg_color(s_tabview, lv_color_hex(0x0A0F1D), 0);

    lv_obj_t *tab1 = lv_tabview_add_tab(s_tabview, LV_SYMBOL_BELL " Clock");
    lv_obj_t *tab2 = lv_tabview_add_tab(s_tabview, LV_SYMBOL_HOME " Sensor");
    lv_obj_t *tab3 = lv_tabview_add_tab(s_tabview, LV_SYMBOL_IMAGE " Photo");
    lv_obj_t *tab4 = lv_tabview_add_tab(s_tabview, LV_SYMBOL_LIST " Novel");
    lv_obj_t *tab5 = lv_tabview_add_tab(s_tabview, LV_SYMBOL_SETTINGS " Sys");

    /* =========================================================================
     * 🌤️ Tab 1: 气象时钟看板
     * ========================================================================= */
    s_label_clock = lv_label_create(tab1);
    lv_label_set_text(s_label_clock, "12:00:00");
    lv_obj_set_style_text_font(s_label_clock, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_label_clock, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(s_label_clock, LV_ALIGN_TOP_MID, 0, 10);

    s_label_date = lv_label_create(tab1);
    lv_label_set_text(s_label_date, "2026-08-25 TUE");
    lv_obj_set_style_text_color(s_label_date, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(s_label_date, LV_ALIGN_TOP_MID, 0, 48);

    s_label_weather = lv_label_create(tab1);
    lv_label_set_text(s_label_weather, ICON_SUNNY " 26.5°C 60%");
    lv_obj_set_style_text_font(s_label_weather, FONT_SUBTITLE, 0);
    lv_obj_set_style_text_color(s_label_weather, lv_color_hex(0xFACC15), 0);
    lv_obj_align(s_label_weather, LV_ALIGN_CENTER, 0, 5);

    s_label_ip = lv_label_create(tab1);
    lv_label_set_text(s_label_ip, "IP: 192.168.4.1");
    lv_obj_set_style_text_color(s_label_ip, lv_color_hex(0x64748B), 0);
    lv_obj_align(s_label_ip, LV_ALIGN_BOTTOM_MID, 0, -10);

    /* =========================================================================
     * 🎛️ Tab 2: 传感器与 IoT 中控
     * ========================================================================= */
    s_label_temp = lv_label_create(tab2);
    lv_label_set_text(s_label_temp, "NTC Temp: 25.0 °C");
    lv_obj_set_style_text_color(s_label_temp, lv_color_hex(0xF87171), 0);
    lv_obj_align(s_label_temp, LV_ALIGN_TOP_LEFT, 5, 5);

    s_bar_temp = lv_bar_create(tab2);
    lv_obj_set_size(s_bar_temp, 210, 10);
    lv_obj_align(s_bar_temp, LV_ALIGN_TOP_LEFT, 5, 25);
    lv_bar_set_range(s_bar_temp, 0, 50);
    lv_bar_set_value(s_bar_temp, 25, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar_temp, lv_color_hex(0xEF4444), LV_PART_INDICATOR);

    s_label_dist = lv_label_create(tab2);
    lv_label_set_text(s_label_dist, "Distance: 20.0 cm");
    lv_obj_set_style_text_color(s_label_dist, lv_color_hex(0x34D399), 0);
    lv_obj_align(s_label_dist, LV_ALIGN_TOP_LEFT, 5, 45);

    s_bar_dist = lv_bar_create(tab2);
    lv_obj_set_size(s_bar_dist, 210, 10);
    lv_obj_align(s_bar_dist, LV_ALIGN_TOP_LEFT, 5, 65);
    lv_bar_set_range(s_bar_dist, 0, 100);
    lv_bar_set_value(s_bar_dist, 20, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar_dist, lv_color_hex(0x10B981), LV_PART_INDICATOR);

    s_btn_led = lv_button_create(tab2);
    lv_obj_set_size(s_btn_led, 210, 36);
    lv_obj_align(s_btn_led, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(s_btn_led, lv_color_hex(0x475569), 0);
    lv_obj_add_event_cb(s_btn_led, on_led_btn_clicked, LV_EVENT_CLICKED, NULL);

    s_label_led = lv_label_create(s_btn_led);
    lv_label_set_text(s_label_led, LV_SYMBOL_POWER " LED: OFF");
    lv_obj_center(s_label_led);

    /* =========================================================================
     * 🖼️ Tab 3: TF 卡电子相册
     * ========================================================================= */
    s_album_label = lv_label_create(tab3);
    lv_label_set_text(s_album_label, "[Photo 1/3]\n/sdcard/photos/wallpaper1.bin");
    lv_obj_set_style_text_color(s_album_label, lv_color_hex(0xE2E8F0), 0);
    lv_obj_set_style_text_align(s_album_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_album_label, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *btn_next_photo = lv_button_create(tab3);
    lv_obj_set_size(btn_next_photo, 160, 36);
    lv_obj_align(btn_next_photo, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(btn_next_photo, on_album_next_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_np = lv_label_create(btn_next_photo);
    lv_label_set_text(lbl_np, LV_SYMBOL_NEXT " Next Photo");
    lv_obj_center(lbl_np);

    /* =========================================================================
     * 📖 Tab 4: TF 卡电子小说阅读器
     * ========================================================================= */
    s_novel_text = lv_label_create(tab4);
    char init_novel_buf[256];
    file_reader_load_novel_page(0, init_novel_buf, sizeof(init_novel_buf), &s_total_novel_pages);
    lv_label_set_text(s_novel_text, init_novel_buf);
    lv_obj_set_style_text_color(s_novel_text, lv_color_hex(0xF1F5F9), 0);
    lv_obj_set_width(s_novel_text, 210);
    lv_obj_align(s_novel_text, LV_ALIGN_TOP_LEFT, 5, 5);

    lv_obj_t *btn_prev_page = lv_button_create(tab4);
    lv_obj_set_size(btn_prev_page, 70, 30);
    lv_obj_align(btn_prev_page, LV_ALIGN_BOTTOM_LEFT, 5, -5);
    lv_obj_add_event_cb(btn_prev_page, on_novel_prev_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lp = lv_label_create(btn_prev_page);
    lv_label_set_text(lp, LV_SYMBOL_PREV);
    lv_obj_center(lp);

    s_novel_page_label = lv_label_create(tab4);
    lv_label_set_text(s_novel_page_label, "Page 1 / 1");
    lv_obj_set_style_text_color(s_novel_page_label, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(s_novel_page_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_t *btn_next_page = lv_button_create(tab4);
    lv_obj_set_size(btn_next_page, 70, 30);
    lv_obj_align(btn_next_page, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
    lv_obj_add_event_cb(btn_next_page, on_novel_next_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ln = lv_label_create(btn_next_page);
    lv_label_set_text(ln, LV_SYMBOL_NEXT);
    lv_obj_center(ln);

    /* =========================================================================
     * ⚙️ Tab 5: 系统与 OTA 管理
     * ========================================================================= */
    s_label_sys_heap = lv_label_create(tab5);
    lv_label_set_text(s_label_sys_heap, "Free Heap: 180 KB");
    lv_obj_set_style_text_color(s_label_sys_heap, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(s_label_sys_heap, LV_ALIGN_TOP_LEFT, 5, 5);

    s_label_sys_psram = lv_label_create(tab5);
    lv_label_set_text(s_label_sys_psram, "PSRAM: 1.8 MB Free");
    lv_obj_set_style_text_color(s_label_sys_psram, lv_color_hex(0xA78BFA), 0);
    lv_obj_align(s_label_sys_psram, LV_ALIGN_TOP_LEFT, 5, 25);

    s_label_ota = lv_label_create(tab5);
    lv_label_set_text(s_label_ota, "OTA Status: Standby");
    lv_obj_set_style_text_color(s_label_ota, lv_color_hex(0xFBBF24), 0);
    lv_obj_align(s_label_ota, LV_ALIGN_TOP_LEFT, 5, 55);

    s_bar_ota = lv_bar_create(tab5);
    lv_obj_set_size(s_bar_ota, 210, 12);
    lv_obj_align(s_bar_ota, LV_ALIGN_TOP_LEFT, 5, 75);
    lv_bar_set_range(s_bar_ota, 0, 100);
    lv_bar_set_value(s_bar_ota, 0, LV_ANIM_OFF);

    bsp_lvgl_port_unlock();
    ESP_LOGI(TAG, "🎨 [UI] LVGL v9 5-App 触控桌面系统初始化成功！");
}

void ui_hub_update_sensor_data(const bsp_sensor_data_t *data)
{
    if (!data) return;
    bsp_lvgl_port_lock(0);

    char buf[64];
    snprintf(buf, sizeof(buf), "NTC Temp: %.1f °C", data->ntc_temperature);
    lv_label_set_text(s_label_temp, buf);
    lv_bar_set_value(s_bar_temp, (int)data->ntc_temperature, LV_ANIM_ON);

    snprintf(buf, sizeof(buf), "Distance: %.1f cm", data->ultrasonic_dist_cm);
    lv_label_set_text(s_label_dist, buf);
    lv_bar_set_value(s_bar_dist, (int)data->ultrasonic_dist_cm, LV_ANIM_ON);

    snprintf(buf, sizeof(buf), "Free Heap: %lu KB", (unsigned long)(data->free_heap_bytes / 1024));
    lv_label_set_text(s_label_sys_heap, buf);

    snprintf(buf, sizeof(buf), "PSRAM: %.1f MB Free", (float)data->free_psram_bytes / (1024.0f * 1024.0f));
    lv_label_set_text(s_label_sys_psram, buf);

    bsp_lvgl_port_unlock();
}

void ui_hub_update_time(const char *time_str)
{
    if (!time_str) return;
    bsp_lvgl_port_lock(0);
    lv_label_set_text(s_label_clock, time_str);
    bsp_lvgl_port_unlock();
}

void ui_hub_update_weather(const hub_weather_info_t *weather)
{
    if (!weather) return;
    bsp_lvgl_port_lock(0);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s %.1f°C %d%%", weather->weather_str, weather->temp, weather->humidity);
    lv_label_set_text(s_label_weather, buf);
    bsp_lvgl_port_unlock();
}

void ui_hub_update_ota_progress(int progress_pct)
{
    bsp_lvgl_port_lock(0);
    char buf[64];
    snprintf(buf, sizeof(buf), "OTA Upgrading: %d%%", progress_pct);
    lv_label_set_text(s_label_ota, buf);
    lv_bar_set_value(s_bar_ota, progress_pct, LV_ANIM_ON);
    bsp_lvgl_port_unlock();
}
