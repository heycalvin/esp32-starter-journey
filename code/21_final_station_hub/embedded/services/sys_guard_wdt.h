#pragma once
#include <stdint.h>
#include "esp_err.h"

esp_err_t sys_guard_wdt_init(uint32_t timeout_ms);
esp_err_t sys_guard_wdt_feed(void);
esp_err_t sys_guard_wdt_subscribe_current_task(void);
