#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYS_STATE_BOOT = 0,
    SYS_STATE_NORMAL,
    SYS_STATE_ALARM,
} sys_state_t;

/**
 * @brief 初始化有限状态机
 */
esp_err_t sys_fsm_init(void);

/**
 * @brief 驱动状态机状态转移
 */
void sys_fsm_handle_event(int event_id);

/**
 * @brief 获取当前系统运行状态
 */
sys_state_t sys_fsm_get_current_state(void);

/**
 * @brief 状态枚举转人类可读字符串
 */
const char* sys_fsm_state_to_str(sys_state_t state);

#ifdef __cplusplus
}
#endif
