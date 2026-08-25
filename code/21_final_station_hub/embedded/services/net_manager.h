#pragma once
#include <stdbool.h>
#include "esp_err.h"

esp_err_t net_manager_init(const char *ssid, const char *password);
bool net_manager_is_wifi_connected(void);
bool net_manager_is_mqtt_connected(void);
void net_manager_get_time_str(char *buf, size_t max_len);
void net_manager_get_ip_str(char *buf, size_t max_len);
esp_err_t net_manager_publish_telemetry(const char *json_payload);
