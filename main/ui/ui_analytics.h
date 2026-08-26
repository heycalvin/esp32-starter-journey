#pragma once
#include "lvgl.h"

/**
 * @brief 在指定 Tab 容器内初始化传感器 24H 趋势图页面
 */
void ui_analytics_init(lv_obj_t *parent_tab);

/**
 * @brief 推送新的传感器数据到折线图（实时滚动）
 * @param temp  温度值 (°C)
 * @param humi  湿度值 (%)
 */
void ui_analytics_push_data(float temp, float humi);
