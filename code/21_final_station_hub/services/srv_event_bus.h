#pragma once

#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(HUB_EVENT_BASE);

typedef enum {
    HUB_EVENT_SENSOR_TICK = 0,
    HUB_EVENT_TIME_SYNCED,
    HUB_EVENT_CMD_TOGGLE_LED,
    HUB_EVENT_CMD_RGB_COLOR,
    HUB_EVENT_PIR_ACTIVITY,
} hub_event_id_t;

esp_err_t srv_event_bus_init(void);
esp_err_t srv_event_bus_post(hub_event_id_t id, const void *data, size_t size);

#ifdef __cplusplus
}
#endif
