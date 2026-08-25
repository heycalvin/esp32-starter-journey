#include "sys_fsm.h"
#include "esp_log.h"

static const char *TAG = "SYS_FSM";
static sys_state_t s_current_state = SYS_STATE_BOOT;

const char* sys_fsm_state_to_str(sys_state_t state)
{
    switch (state) {
        case SYS_STATE_BOOT:    return "BOOT (开机自检)";
        case SYS_STATE_STANDBY: return "STANDBY (低功耗待机)";
        case SYS_STATE_ACTIVE:  return "ACTIVE (活跃运行)";
        case SYS_STATE_ALARM:   return "ALARM (危险告警)";
        default:                return "UNKNOWN";
    }
}

esp_err_t sys_fsm_init(void)
{
    s_current_state = SYS_STATE_BOOT;
    ESP_LOGI(TAG, "⚙️ [服务层] 有限状态机 (FSM) 模块就绪，初始状态: %s", sys_fsm_state_to_str(s_current_state));
    return ESP_OK;
}

void sys_fsm_handle_event(fsm_event_t event)
{
    sys_state_t prev_state = s_current_state;

    switch (s_current_state) {
        case SYS_STATE_BOOT:
            if (event == FSM_EVT_SELF_TEST_OK) {
                s_current_state = SYS_STATE_STANDBY;
            }
            break;

        case SYS_STATE_STANDBY:
            if (event == FSM_EVT_USER_ACTIVITY) {
                s_current_state = SYS_STATE_ACTIVE;
            } else if (event == FSM_EVT_SENSOR_OVERLIMIT) {
                s_current_state = SYS_STATE_ALARM;
            }
            break;

        case SYS_STATE_ACTIVE:
            if (event == FSM_EVT_TIMEOUT_IDLE) {
                s_current_state = SYS_STATE_STANDBY;
            } else if (event == FSM_EVT_SENSOR_OVERLIMIT) {
                s_current_state = SYS_STATE_ALARM;
            }
            break;

        case SYS_STATE_ALARM:
            if (event == FSM_EVT_ALARM_CLEAR) {
                s_current_state = SYS_STATE_STANDBY;
            }
            break;

        default:
            break;
    }

    if (prev_state != s_current_state) {
        ESP_LOGI(TAG, "🔄 [FSM 状态跃迁] \033[33m%s\033[0m ➔➔➔ \033[1;32m%s\033[0m",
                 sys_fsm_state_to_str(prev_state), sys_fsm_state_to_str(s_current_state));
    }
}

sys_state_t sys_fsm_get_current_state(void)
{
    return s_current_state;
}
