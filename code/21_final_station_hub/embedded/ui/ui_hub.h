#pragma once
#include <stdbool.h>
#include "bsp_sensor.h"
#include "sys_event_bus.h"

void ui_hub_init(void);
void ui_hub_update_sensor_data(const bsp_sensor_data_t *data);
void ui_hub_update_time(const char *time_str);
void ui_hub_update_weather(const hub_weather_info_t *weather);
void ui_hub_update_ota_progress(int progress_pct);
void ui_hub_show_toast(const char *msg);
