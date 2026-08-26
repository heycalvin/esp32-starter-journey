#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

typedef enum {
    NET_MODE_PROVISIONING_AP = 0, // AP 配网模式 (发射热点等待配置)
    NET_MODE_CONNECTING_STA,      // 正在连接路由器
    NET_MODE_CONNECTED_STA        // 已成功连接路由器
} net_mode_t;

esp_err_t net_manager_init(const char *fallback_ssid, const char *fallback_password);
net_mode_t net_manager_get_mode(void);
bool net_manager_is_provisioning(void);
bool net_manager_is_wifi_connected(void);
bool net_manager_is_mqtt_connected(void);
void net_manager_get_time_str(char *buf, size_t max_len);
void net_manager_get_date_str(char *buf, size_t max_len);
void net_manager_get_uptime_str(char *buf, size_t max_len);
void net_manager_get_ip_str(char *buf, size_t max_len);
void net_manager_get_location_str(char *buf, size_t max_len);
void net_manager_set_location_str(const char *loc_str);
void net_manager_get_weather_str(char *buf, size_t max_len);
void net_manager_set_weather_str(const char *w_str);
esp_err_t net_manager_save_credentials(const char *ssid, const char *password);
esp_err_t net_manager_reset_credentials(void);
esp_err_t net_manager_publish_telemetry(const char *json_payload);
