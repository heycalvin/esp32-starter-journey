#pragma once
#include "esp_err.h"

typedef enum {
    HUB_STATE_BOOT = 0,
    HUB_STATE_WEATHER_CLOCK, // 页面 1: 气象时钟看板
    HUB_STATE_SENSOR_HUB,    // 页面 2: 传感器 IoT 仪表盘
    HUB_STATE_PHOTO_ALBUM,   // 页面 3: TF 卡电子相册
    HUB_STATE_NOVEL_READER,  // 页面 4: TF 卡电子小说阅读器
    HUB_STATE_SYSTEM_CENTER, // 页面 5: 系统与无线中控
    HUB_STATE_OTA_UPGRADING, // OTA 固件升级进行中
    HUB_STATE_ALARM_PANIC,   // 异常越限警报
} hub_state_t;

typedef enum {
    HUB_FSM_EVT_INIT_DONE = 0,
    HUB_FSM_EVT_SWITCH_APP,
    HUB_FSM_EVT_START_OTA,
    HUB_FSM_EVT_FINISH_OTA,
    HUB_FSM_EVT_TRIGGER_ALARM,
    HUB_FSM_EVT_CLEAR_ALARM,
} hub_fsm_event_t;

esp_err_t sys_fsm_init(void);
void sys_fsm_handle_event(hub_fsm_event_t event, int param);
hub_state_t sys_fsm_get_state(void);
const char* sys_fsm_state_to_str(hub_state_t state);
