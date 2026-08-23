#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t srv_mqtt_start(const char *broker_uri);
esp_err_t srv_mqtt_publish_telemetry(const char *json_data);
bool srv_mqtt_is_connected(void);

#ifdef __cplusplus
}
#endif
