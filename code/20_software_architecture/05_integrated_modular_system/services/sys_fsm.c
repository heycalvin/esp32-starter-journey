#include "sys_fsm.h"
#include "sys_event_bus.h"
#include "esp_log.h"

static const char *TAG = "SYS_FSM";

static sys_state_t s_current_state = SYS_STATE_BOOT;

const char* sys_fsm_state_to_str(sys_state_t state)
{
    switch (state) {
        case SYS_STATE_BOOT:   return "BOOT (系统自检)";
        case SYS_STATE_NORMAL: return "NORMAL (平稳运行)";
        case SYS_STATE_ALARM:  return "ALARM (警戒报警)";
        default:               return "UNKNOWN";
    }
}

esp_err_t sys_fsm_init(void)
{
    s_current_state = SYS_STATE_NORMAL;
    ESP_LOGI(TAG, "✅ 有限状态机 (FSM) 初始化完成，当前状态: %s", sys_fsm_state_to_str(s_current_state));
    return ESP_OK;
}

void sys_fsm_handle_event(int event_id)
{
    sys_state_t prev = s_current_state;
    if (event_id == SYS_EVENT_ALARM_TRIGGERED) {
        s_current_state = (s_current_state == SYS_STATE_ALARM) ? SYS_STATE_NORMAL : SYS_STATE_ALARM;
    }
    ESP_LOGI(TAG, "🔄 [FSM 状态转移] %s ➔➔➔ %s", sys_fsm_state_to_str(prev), sys_fsm_state_to_str(s_current_state));
}

sys_state_t sys_fsm_get_current_state(void)
{
    return s_current_state;
}
