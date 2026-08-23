#pragma once

#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(SYS_EVENT_BASE);

typedef enum {
    SYS_EVENT_BUTTON_PRESSED = 0,
    SYS_EVENT_SENSOR_UPDATED,
    SYS_EVENT_ALARM_TRIGGERED,
} sys_event_id_t;

/**
 * @brief 初始化系统全局事件总线
 */
esp_err_t sys_event_bus_init(void);

/**
 * @brief 向事件总线分发广播事件
 */
esp_err_t sys_event_bus_post(sys_event_id_t event_id, const void *event_data, size_t event_data_size);

#ifdef __cplusplus
}
#endif
