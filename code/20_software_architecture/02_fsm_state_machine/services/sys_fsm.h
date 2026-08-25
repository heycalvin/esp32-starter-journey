#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 1. 系统核心生命周期状态枚举 */
typedef enum {
    SYS_STATE_BOOT = 0,   // 开机自检中
    SYS_STATE_STANDBY,    // 低功耗待机守候
    SYS_STATE_ACTIVE,     // 活跃工作运行
    SYS_STATE_ALARM,      // 异常危险告警
    SYS_STATE_MAX
} sys_state_t;

/* 2. 状态机触发事件枚举 */
typedef enum {
    FSM_EVT_SELF_TEST_OK = 0,  // 自检完成通过
    FSM_EVT_USER_ACTIVITY,     // 用户交互唤醒 (触摸/按键)
    FSM_EVT_TIMEOUT_IDLE,      // 空闲超时自动休眠
    FSM_EVT_SENSOR_OVERLIMIT,  // 传感器越限异常
    FSM_EVT_ALARM_CLEAR        // 人工确认解除告警
} fsm_event_t;

/**
 * @brief 初始化有限状态机 (FSM)
 */
esp_err_t sys_fsm_init(void);

/**
 * @brief 投递事件并执行状态机迁移
 */
void sys_fsm_handle_event(fsm_event_t event);

/**
 * @brief 获取当前系统状态
 */
sys_state_t sys_fsm_get_current_state(void);

/**
 * @brief 获取状态文本名称
 */
const char* sys_fsm_state_to_str(sys_state_t state);

#ifdef __cplusplus
}
#endif
