#include "sys_fsm.h"
#include "esp_log.h"

static const char *TAG = "SYS_FSM";
static hub_state_t s_current_state = HUB_STATE_BOOT;

const char* sys_fsm_state_to_str(hub_state_t state)
{
    switch (state) {
        case HUB_STATE_BOOT:          return "BOOTING (系统启动自检)";
        case HUB_STATE_WEATHER_CLOCK: return "APP1_WEATHER_CLOCK (气象时钟看板)";
        case HUB_STATE_SENSOR_HUB:    return "APP2_SENSOR_HUB (传感器中控台)";
        case HUB_STATE_PHOTO_ALBUM:   return "APP3_PHOTO_ALBUM (电子相册模式)";
        case HUB_STATE_NOVEL_READER:  return "APP4_NOVEL_READER (电子小说模式)";
        case HUB_STATE_SYSTEM_CENTER: return "APP5_SYSTEM_CENTER (系统设置与OTA)";
        case HUB_STATE_OTA_UPGRADING: return "OTA_UPGRADING (OTA无线刷机中)";
        case HUB_STATE_ALARM_PANIC:   return "ALARM_PANIC (异常越限警报)";
        default:                      return "UNKNOWN";
    }
}

esp_err_t sys_fsm_init(void)
{
    s_current_state = HUB_STATE_BOOT;
    ESP_LOGI(TAG, "⚙️ [服务层] 全局有限状态机 (FSM) 初始化就绪 (初始状态: %s)", sys_fsm_state_to_str(s_current_state));
    return ESP_OK;
}

void sys_fsm_handle_event(hub_fsm_event_t event, int param)
{
    hub_state_t prev = s_current_state;

    switch (event) {
        case HUB_FSM_EVT_INIT_DONE:
            s_current_state = HUB_STATE_WEATHER_CLOCK;
            break;

        case HUB_FSM_EVT_SWITCH_APP:
            if (param >= HUB_STATE_WEATHER_CLOCK && param <= HUB_STATE_SYSTEM_CENTER) {
                s_current_state = (hub_state_t)param;
            }
            break;

        case HUB_FSM_EVT_START_OTA:
            s_current_state = HUB_STATE_OTA_UPGRADING;
            break;

        case HUB_FSM_EVT_FINISH_OTA:
            s_current_state = HUB_STATE_SYSTEM_CENTER;
            break;

        case HUB_FSM_EVT_TRIGGER_ALARM:
            s_current_state = HUB_STATE_ALARM_PANIC;
            break;

        case HUB_FSM_EVT_CLEAR_ALARM:
            s_current_state = HUB_STATE_WEATHER_CLOCK;
            break;

        default:
            break;
    }

    if (prev != s_current_state) {
        ESP_LOGI(TAG, "🔄 [FSM 跃迁] %s ──► %s", sys_fsm_state_to_str(prev), sys_fsm_state_to_str(s_current_state));
    }
}

hub_state_t sys_fsm_get_state(void)
{
    return s_current_state;
}
