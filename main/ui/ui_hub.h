#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "bsp_sensor.h"
#include "sys_event_bus.h"

void ui_hub_init(void);
/**
 * @brief 处理 SW3 短按：在首页打开程序列表，在其他页面返回首页。
 */
void ui_hub_handle_sw3_short_press(void);
void ui_hub_update_sensor_data(const bsp_sensor_data_t *data);
void ui_hub_update_time_and_date(const char *time_str, const char *date_str);
void ui_hub_update_weather_full(const char *location, const char *weather_desc, float temp, float humi);
void ui_hub_update_system_status(const char *uptime_str, const char *ip_str, uint32_t heap, uint32_t psram);
void ui_hub_update_ota_progress(int progress_pct);
void ui_hub_show_toast(const char *msg);
