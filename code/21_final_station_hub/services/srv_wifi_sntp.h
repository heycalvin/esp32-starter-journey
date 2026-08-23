#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t srv_wifi_sntp_start(const char *ssid, const char *password);
bool srv_wifi_is_connected(void);
void srv_wifi_get_time_str(char *buf, size_t max_len);

#ifdef __cplusplus
}
#endif
