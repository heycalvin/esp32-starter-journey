#pragma once
#include <stdbool.h>
#include "esp_err.h"

esp_err_t ble_manager_init(const char *device_name);
bool ble_manager_is_connected(void);
esp_err_t ble_manager_send_sensor_notify(float temp, float humi, float dist);
