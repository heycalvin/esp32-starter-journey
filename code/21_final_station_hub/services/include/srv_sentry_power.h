#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t srv_sentry_power_init(void);
void srv_sentry_feed_dog(void);

#ifdef __cplusplus
}
#endif
