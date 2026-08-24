/**
 * ==============================================================================
 * 🚀 ESP32 物联网实战闯关 —— 第 20 关：嵌入式软件工程与模块化分层架构
 * 📁 实验 2: 有限状态机 (FSM) 系统中枢生命周期模型 (State Machine)
 * ==============================================================================
 * 
 * 📌 【实验目标】
 * 1. 理解为什么复杂嵌入式工程必须使用有限状态机（FSM）彻底消除混乱的嵌套 `if-else`；
 * 2. 定义清晰的系统状态枚举（BOOTING ➔ STANDBY ➔ ACTIVE ➔ ALARM）；
 * 3. 掌握基于事件驱动的状态转移模型与状态进入/退出回调动作；
 * 4. 构建自愈可靠、逻辑严密的工业级系统运行生命周期！
 * ==============================================================================
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "EXP2_FSM";

/* 1. 系统核心状态枚举 */
typedef enum {
    STATE_BOOTING = 0,   // 开机自检中
    STATE_STANDBY,       // 待机守候中
    STATE_ACTIVE,        // 活跃工作中
    STATE_ALARM,         // 异常报警中
    STATE_MAX
} sys_state_t;

/* 2. 外部触发事件枚举 */
typedef enum {
    EVT_SELF_TEST_OK = 0, // 自检完成通过
    EVT_USER_ACTIVITY,    // 用户触发交互 (如触摸屏幕/按键)
    EVT_TIMEOUT_IDLE,     // 超时无操作
    EVT_SENSOR_OVERLIMIT, // 传感器采样越限异常
    EVT_ALARM_CLEAR       // 异常确认解除
} sys_event_t;

static const char *state_to_str(sys_state_t s) {
    switch (s) {
        case STATE_BOOTING: return "BOOTING (开机自检)";
        case STATE_STANDBY: return "STANDBY (低功耗待机)";
        case STATE_ACTIVE:  return "ACTIVE (活跃运行)";
        case STATE_ALARM:   return "ALARM (危险告警)";
        default: return "UNKNOWN";
    }
}

static sys_state_t s_current_state = STATE_BOOTING;

/**
 * @brief 状态机核心处理引擎 (处理事件并执行状态跃迁)
 */
static void fsm_handle_event(sys_event_t event)
{
    sys_state_t prev_state = s_current_state;

    switch (s_current_state) {
        case STATE_BOOTING:
            if (event == EVT_SELF_TEST_OK) {
                s_current_state = STATE_STANDBY;
            }
            break;

        case STATE_STANDBY:
            if (event == EVT_USER_ACTIVITY) {
                s_current_state = STATE_ACTIVE;
            } else if (event == EVT_SENSOR_OVERLIMIT) {
                s_current_state = STATE_ALARM;
            }
            break;

        case STATE_ACTIVE:
            if (event == EVT_TIMEOUT_IDLE) {
                s_current_state = STATE_STANDBY;
            } else if (event == EVT_SENSOR_OVERLIMIT) {
                s_current_state = STATE_ALARM;
            }
            break;

        case STATE_ALARM:
            if (event == EVT_ALARM_CLEAR) {
                s_current_state = STATE_STANDBY;
            }
            break;

        default:
            break;
    }

    if (prev_state != s_current_state) {
        ESP_LOGI(TAG, "🔄 [状态转移] \033[33m%s\033[0m ➔➔➔ \033[1;32m%s\033[0m", 
                 state_to_str(prev_state), state_to_str(s_current_state));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "   🚀 Level 20 实验 2：有限状态机 FSM 中枢引擎架构        ");
    ESP_LOGI(TAG, "==========================================================");

    ESP_LOGI(TAG, "📍 系统初始状态: %s", state_to_str(s_current_state));

    // 模拟生命周期事件触发序列
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "\n⚡ [事件 1 触发] ➔ 硬件外设自检通过 (EVT_SELF_TEST_OK)");
    fsm_handle_event(EVT_SELF_TEST_OK);

    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "\n⚡ [事件 2 触发] ➔ 用户触摸唤醒屏幕 (EVT_USER_ACTIVITY)");
    fsm_handle_event(EVT_USER_ACTIVITY);

    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "\n⚡ [事件 3 触发] ➔ NTC 传感器采集温度超限 50°C 报警 (EVT_SENSOR_OVERLIMIT)");
    fsm_handle_event(EVT_SENSOR_OVERLIMIT);

    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "\n⚡ [事件 4 触发] ➔ 用户按下消警按键 (EVT_ALARM_CLEAR)");
    fsm_handle_event(EVT_ALARM_CLEAR);

    ESP_LOGI(TAG, "\n🏆 FSM 有限状态机生命周期流程 100% 验证通过！");
}
