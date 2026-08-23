#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t srv_ble_start(const char *device_name);

#ifdef __cplusplus
}
#endif
