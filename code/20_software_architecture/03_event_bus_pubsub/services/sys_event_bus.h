#pragma once

#include "esp_event.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 1. 声明系统全局事件基底 (Event Base) */
ESP_EVENT_DECLARE_BASE(SYS_EVENT_BASE);

/* 2. 事件 ID 枚举 */
typedef enum {
    SYS_EVENT_TEMP_UPDATED = 0,  // 温度更新事件
    SYS_EVENT_HUMI_UPDATED,      // 湿度更新事件
    SYS_EVENT_ALARM_TRIGGERED,   // 异常报警事件
} sys_event_id_t;

/* 3. 伴随事件传递的载荷数据结构体 */
typedef struct {
    float temperature;
    float humidity;
    uint32_t timestamp_ms;
} sensor_event_payload_t;

/**
 * @brief 初始化系统全局统一事件总线
 */
esp_err_t sys_event_bus_init(void);

/**
 * @brief 向事件总线广播事件
 */
esp_err_t sys_event_bus_post(sys_event_id_t event_id, const void *event_data, size_t event_data_size);

#ifdef __cplusplus
}
#endif
