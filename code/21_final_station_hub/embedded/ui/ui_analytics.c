/**
 * @file ui_analytics.c
 * @brief 传感器 24H 实时动态趋势折线图
 *
 * 使用 LVGL 9 原生 lv_chart 折线图组件：
 * - 两条数据系列：温度（荧光红）与 湿度（冰蓝）
 * - 最多保存 24 个数据点（对应 24 次采集周期）
 * - 顶部显示当日极值（最高温/最低温/最高湿）
 * - 实时推入新数据后自动滚动最旧数据点
 */
#include "ui_analytics.h"
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "bsp_lvgl_port.h"
#include "sys_font_manager.h"
#include "esp_log.h"

static const char *TAG = "UI_ANALYTICS";

#define DATA_POINTS  24   // 保留 24 个历史数据点

static lv_obj_t *s_chart         = NULL;
static lv_chart_series_t *s_ser_temp = NULL;
static lv_chart_series_t *s_ser_humi = NULL;

// 实时极值统计
static float s_max_temp = -99.f, s_min_temp = 99.f;
static float s_max_humi = 0.f,   s_min_humi = 100.f;
static int   s_data_count = 0;

static lv_obj_t *s_lbl_max_temp  = NULL;
static lv_obj_t *s_lbl_min_temp  = NULL;
static lv_obj_t *s_lbl_max_humi  = NULL;
static lv_obj_t *s_lbl_avg_temp  = NULL;

// 累积计算均值
static float s_sum_temp = 0.f;
static float s_sum_humi = 0.f;

void ui_analytics_init(lv_obj_t *parent_tab)
{
    if (!parent_tab) return;
    const lv_font_t *font_cn = sys_font_manager_get_font(14);

    // 页面标题
    lv_obj_t *title = lv_label_create(parent_tab);
    lv_label_set_text(title, "SENSOR ANALYTICS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x64748B), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    /* ── 极值统计卡片（4 等分横排，Y: 18，高 32） ─────────────── */
    typedef struct { const char *label; const char *val; uint32_t color; lv_obj_t **out; } StatCard;
    StatCard stats[] = {
        {"最高温", "--.-°C", 0xF87171, &s_lbl_max_temp},
        {"最低温", "--.-°C", 0x38BDF8, &s_lbl_min_temp},
        {"最高湿", "--.-%",  0x34D399, &s_lbl_max_humi},
        {"均值温", "--.-°C", 0xFBBF24, &s_lbl_avg_temp},
    };
    int card_w = 54;
    for (int i = 0; i < 4; i++) {
        lv_obj_t *card = lv_obj_create(parent_tab);
        lv_obj_set_size(card, card_w, 34);
        lv_obj_set_pos(card, 2 + i * (card_w + 2), 18);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x0F1B2D), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x1E3A5F), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 6, 0);
        lv_obj_set_style_pad_all(card, 2, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl_name = lv_label_create(card);
        lv_label_set_text(lbl_name, stats[i].label);
        lv_obj_set_style_text_font(lbl_name, font_cn, 0);
        lv_obj_set_style_text_color(lbl_name, lv_color_hex(0x64748B), 0);
        lv_obj_align(lbl_name, LV_ALIGN_TOP_MID, 0, 0);

        lv_obj_t *lbl_val = lv_label_create(card);
        lv_label_set_text(lbl_val, stats[i].val);
        lv_obj_set_style_text_font(lbl_val, font_cn, 0);
        lv_obj_set_style_text_color(lbl_val, lv_color_hex(stats[i].color), 0);
        lv_obj_align(lbl_val, LV_ALIGN_BOTTOM_MID, 0, 0);
        *stats[i].out = lbl_val;
    }

    /* ── 折线图主卡片（Y: 56，高 130，宽 228） ─────────────────── */
    lv_obj_t *chart_card = lv_obj_create(parent_tab);
    lv_obj_set_size(chart_card, 228, 130);
    lv_obj_set_pos(chart_card, 6, 56);
    lv_obj_set_style_bg_color(chart_card, lv_color_hex(0x060E1A), 0);
    lv_obj_set_style_border_color(chart_card, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_border_width(chart_card, 1, 0);
    lv_obj_set_style_radius(chart_card, 8, 0);
    lv_obj_set_style_pad_all(chart_card, 4, 0);
    lv_obj_clear_flag(chart_card, LV_OBJ_FLAG_SCROLLABLE);

    s_chart = lv_chart_create(chart_card);
    lv_obj_set_size(s_chart, 218, 120);
    lv_obj_center(s_chart);
    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, DATA_POINTS);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_div_line_count(s_chart, 4, 0);
    lv_obj_set_style_bg_color(s_chart, lv_color_hex(0x060E1A), 0);
    lv_obj_set_style_border_width(s_chart, 0, 0);
    lv_obj_set_style_line_color(s_chart, lv_color_hex(0x1E293B), LV_PART_MAIN);
    lv_chart_set_update_mode(s_chart, LV_CHART_UPDATE_MODE_SHIFT);

    // 温度系列（荧光红）
    s_ser_temp = lv_chart_add_series(s_chart,
        lv_color_hex(0xF87171), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_line_width(s_chart, 2, LV_PART_ITEMS);

    // 湿度系列（冰蓝）
    s_ser_humi = lv_chart_add_series(s_chart,
        lv_color_hex(0x38BDF8), LV_CHART_AXIS_PRIMARY_Y);

    // 初始化数据点为无效值（LV_CHART_POINT_NONE）
    for (int i = 0; i < DATA_POINTS; i++) {
        lv_chart_set_next_value(s_chart, s_ser_temp, LV_CHART_POINT_NONE);
        lv_chart_set_next_value(s_chart, s_ser_humi, LV_CHART_POINT_NONE);
    }

    /* ── 图例说明（Y: 190，高 22） ─────────────────────────────── */
    lv_obj_t *legend_row = lv_obj_create(parent_tab);
    lv_obj_set_size(legend_row, 228, 24);
    lv_obj_set_pos(legend_row, 6, 190);
    lv_obj_set_style_bg_color(legend_row, lv_color_hex(0x0A1220), 0);
    lv_obj_set_style_border_color(legend_row, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(legend_row, 1, 0);
    lv_obj_set_style_radius(legend_row, 6, 0);
    lv_obj_set_style_pad_all(legend_row, 3, 0);
    lv_obj_clear_flag(legend_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_temp_legend = lv_label_create(legend_row);
    lv_label_set_text(lbl_temp_legend, "— 温度 (°C)");
    lv_obj_set_style_text_font(lbl_temp_legend, font_cn, 0);
    lv_obj_set_style_text_color(lbl_temp_legend, lv_color_hex(0xF87171), 0);
    lv_obj_align(lbl_temp_legend, LV_ALIGN_LEFT_MID, 4, 0);

    lv_obj_t *lbl_humi_legend = lv_label_create(legend_row);
    lv_label_set_text(lbl_humi_legend, "— 湿度 (%)");
    lv_obj_set_style_text_font(lbl_humi_legend, font_cn, 0);
    lv_obj_set_style_text_color(lbl_humi_legend, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(lbl_humi_legend, LV_ALIGN_RIGHT_MID, -4, 0);

    ESP_LOGI(TAG, "传感器趋势图初始化完毕");
}

void ui_analytics_push_data(float temp, float humi)
{
    if (!s_chart || !s_ser_temp || !s_ser_humi) return;

    bsp_lvgl_port_lock(0);

    // 更新折线图（SHIFT 模式自动移除最旧数据）
    lv_chart_set_next_value(s_chart, s_ser_temp, (int32_t)(temp * 1.0f));
    lv_chart_set_next_value(s_chart, s_ser_humi, (int32_t)(humi * 1.0f));
    lv_chart_refresh(s_chart);

    // 更新极值与均值统计
    s_data_count++;
    s_sum_temp += temp;
    s_sum_humi += humi;
    if (temp > s_max_temp) s_max_temp = temp;
    if (temp < s_min_temp) s_min_temp = temp;
    if (humi > s_max_humi) s_max_humi = humi;

    char buf[24];
    if (s_lbl_max_temp) {
        snprintf(buf, sizeof(buf), "%.1f°C", s_max_temp);
        lv_label_set_text(s_lbl_max_temp, buf);
    }
    if (s_lbl_min_temp) {
        snprintf(buf, sizeof(buf), "%.1f°C", s_min_temp);
        lv_label_set_text(s_lbl_min_temp, buf);
    }
    if (s_lbl_max_humi) {
        snprintf(buf, sizeof(buf), "%.1f%%", s_max_humi);
        lv_label_set_text(s_lbl_max_humi, buf);
    }
    if (s_lbl_avg_temp && s_data_count > 0) {
        snprintf(buf, sizeof(buf), "%.1f°C", s_sum_temp / s_data_count);
        lv_label_set_text(s_lbl_avg_temp, buf);
    }

    bsp_lvgl_port_unlock();
}
