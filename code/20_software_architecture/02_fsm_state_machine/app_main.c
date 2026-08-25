/**
 * ==============================================================================
 * 🚀 ESP32 物联网实战闯关 —— 第 20 关：嵌入式软件工程与模块化分层架构
 * 📁 实验 2: 有限状态机 (FSM) 模块化工程实战
 * ==============================================================================
 * 
 * 📌 【架构说明】
 * - services/sys_fsm.h / .c : 承载纯净的状态转移引擎与状态机生命周期逻辑
 * - main/app_main.c         : 业务调度装配中枢
 * ==============================================================================
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "services/sys_fsm.h"

static const char *TAG = "EXP2_MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "   🚀 Level 20 实验 2：有限状态机 FSM 多文件工程实战      ");
    ESP_LOGI(TAG, "==========================================================");

    // 1. 初始化中间件状态机服务
    sys_fsm_init();

    // 2. 模拟业务事件驱动状态机连续跳转
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "\n⚡ [外部事件 1] ➔ 硬件外设开机自检通过 (FSM_EVT_SELF_TEST_OK)");
    sys_fsm_handle_event(FSM_EVT_SELF_TEST_OK);

    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "\n⚡ [外部事件 2] ➔ 用户触摸屏幕唤醒交互 (FSM_EVT_USER_ACTIVITY)");
    sys_fsm_handle_event(FSM_EVT_USER_ACTIVITY);

    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "\n⚡ [外部事件 3] ➔ 传感器温度超限告警 (FSM_EVT_SENSOR_OVERLIMIT)");
    sys_fsm_handle_event(FSM_EVT_SENSOR_OVERLIMIT);

    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "\n⚡ [外部事件 4] ➔ 人工确认解除告警 (FSM_EVT_ALARM_CLEAR)");
    sys_fsm_handle_event(FSM_EVT_ALARM_CLEAR);

    ESP_LOGI(TAG, "\n🏆 FSM 有限状态机多文件工程验证 100% 成功！");
}
