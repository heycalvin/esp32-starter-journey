/**
 * ==============================================================================
 * 🚀 ESP32 物联网实战闯关 —— 第 19 关：低功耗电源管理与 Deep-sleep 深度睡眠
 * 📁 实验 3: 综合大工程 —— 微安级智能野外环境监测哨兵 (Ultra Low-Power Sentry)
 * ==============================================================================
 * 
 * 📌 【系统架构与设计思想】
 * 1. 业务场景：电池供电的野外智能哨兵，平时芯片处于 5μA 深度睡眠；
 * 2. 双重唤醒机制：
 *    - 周期性巡检（Timer 10s 闹钟）：每 10 秒自动苏醒 200ms，采集 NTC 温度并记录心跳；
 *    - 突发入侵告警（EXT1 红外/按键）：有人靠近或按键按下瞬间强行唤醒，触发告警灯并记录入侵事件；
 * 3. 极速闪退技术（Fast Wake-to-Sleep）：200ms 内完成采集、计算与 RTC 数据记录，秒级重新入睡，省电 99.8%！
 * 
 * 📌 【硬件引脚说明】
 * - 板载 LED2    : GPIO27 (巡检闪烁 1 次，入侵报警连闪 3 次)
 * - 用户按键 SW3 : GPIO39 (EXT0 唤醒)
 * - 人体红外 SR602: GPIO34 (EXT1 唤醒)
 * ==============================================================================
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

#define PIN_LED             GPIO_NUM_27
#define PIN_BUTTON_SW3      GPIO_NUM_39
#define PIN_PIR_SR602       GPIO_NUM_34
#define HEARTBEAT_INTERVAL_SEC 10 // 周期性心跳巡检间隔 (秒)

static const char *TAG = "EXP3_LP_SENTRY";

// -----------------------------------------------------------------------------
// 🧠 RTC 慢速内存持久化数据区 (Deep-sleep 期间不丢失)
// -----------------------------------------------------------------------------
typedef struct {
    int total_cycles;       // 总运行轮次
    int timer_checks;       // 定时常规巡检次数
    int intrusion_alerts;   // 人体入侵告警次数
    int last_temp_raw;      // 最近一次模拟采样值
} sentry_telemetry_t;

static RTC_DATA_ATTR sentry_telemetry_t s_sentry_data = {0};

void app_main(void)
{
    int64_t wake_start_us = esp_timer_get_time();
    s_sentry_data.total_cycles++;

    // 初始化 LED2 引脚
    gpio_reset_pin(PIN_LED);
    gpio_set_direction(PIN_LED, GPIO_MODE_OUTPUT);

    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "   🛡️ Level 19 实验 3：微安级智能野外环境监测哨兵启动     ");
    ESP_LOGI(TAG, "==========================================================");

    // 1. 诊断唤醒源并分流处理业务
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    bool is_intrusion = false;

    if (cause == ESP_SLEEP_WAKEUP_TIMER) {
        s_sentry_data.timer_checks++;
        ESP_LOGI(TAG, "⏰ 【常规心跳巡检】: 定时器唤醒 (第 %d 次常规巡检)", s_sentry_data.timer_checks);

        // 快速模拟温度采集（仅耗时 20ms）
        s_sentry_data.last_temp_raw = 24 + (s_sentry_data.total_cycles % 5);
        ESP_LOGI(TAG, "🌡️ 采集当前环境温度: %d °C (状态正常)", s_sentry_data.last_temp_raw);

        // LED 快速微弱闪烁 1 次 (50ms)
        gpio_set_level(PIN_LED, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(PIN_LED, 0);

    } else if (cause == ESP_SLEEP_WAKEUP_EXT0 || cause == ESP_SLEEP_WAKEUP_EXT1) {
        s_sentry_data.intrusion_alerts++;
        is_intrusion = true;
        ESP_LOGW(TAG, "🚨 \033[1;31m【突发安全告警】: 检测到人体靠近或按键强行触发！(第 %d 次告警)\033[0m",
                 s_sentry_data.intrusion_alerts);

        // 告警状态：LED 强力快闪 3 次 (警示入侵者)
        for (int i = 0; i < 3; i++) {
            gpio_set_level(PIN_LED, 1);
            vTaskDelay(pdMS_TO_TICKS(80));
            gpio_set_level(PIN_LED, 0);
            vTaskDelay(pdMS_TO_TICKS(80));
        }

    } else {
        ESP_LOGI(TAG, "🔌 【哨兵上电部署】: 系统初始化完成，进入守卫阵地！");
    }

    // 2. 打印哨兵持久化黑匣子状态
    ESP_LOGI(TAG, "----------------------------------------------------------");
    ESP_LOGI(TAG, "📊 【哨兵运行黑匣子 (RTC 内存)】:");
    ESP_LOGI(TAG, "   • 累计苏醒总轮次 : %d 轮", s_sentry_data.total_cycles);
    ESP_LOGI(TAG, "   • 定时常规巡检   : %d 次", s_sentry_data.timer_checks);
    ESP_LOGI(TAG, "   • 异常入侵拦截   : %d 次", s_sentry_data.intrusion_alerts);
    ESP_LOGI(TAG, "   • 最近一次温度   : %d °C", s_sentry_data.last_temp_raw);
    ESP_LOGI(TAG, "----------------------------------------------------------");

    // 3. 配置双重休眠唤醒源 (定时 10s + 外部按键/红外)
    esp_sleep_enable_timer_wakeup((uint64_t)HEARTBEAT_INTERVAL_SEC * 1000000ULL);
    esp_sleep_enable_ext0_wakeup(PIN_BUTTON_SW3, 0);
    esp_sleep_enable_ext1_wakeup((1ULL << PIN_PIR_SR602), ESP_EXT1_WAKEUP_ANY_HIGH);

    int64_t elapsed_ms = (esp_timer_get_time() - wake_start_us) / 1000;
    ESP_LOGI(TAG, "⚡ 【能效分析】: 本次苏醒处理总耗时: \033[1;32m%lld ms\033[0m", (long long)elapsed_ms);
    ESP_LOGI(TAG, "💤 哨兵重新进入 5μA 极低功耗沉睡态...\n");

    // 4. 立即重新沉睡
    esp_deep_sleep_start();
}
