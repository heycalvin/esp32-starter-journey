/**
 * ==============================================================================
 * 🚀 ESP32 物联网实战闯关 —— 第 19 关：低功耗电源管理与 Deep-sleep 深度睡眠
 * 📁 实验 2: EXT0 (按键 SW3) 与 EXT1 (红外 SR602) 双外部硬件电平中断瞬间唤醒
 * ==============================================================================
 * 
 * 📌 【实验目标】
 * 1. 掌握 EXT0（单引脚 RTC GPIO 电平触发）与 EXT1（多引脚 RTC GPIO 掩码触发）的区别；
 * 2. 配置 SW3 按键（GPIO39）低电平唤醒，实现“按键即开机/唤醒”的智能遥控器体验；
 * 3. 配置 SR602 人体红外感应（GPIO34）高电平唤醒，实现“有人靠近秒级感应开机”；
 * 4. 使用 esp_sleep_get_wakeup_cause() 与 esp_sleep_get_ext1_wakeup_status() 诊断精准唤醒源！
 * 
 * 📌 【硬件引脚说明】
 * - 用户按键 SW3 : GPIO39 (纯输入专用，低电平有效，RTC_GPIO3) ➔ 绑定 EXT0 唤醒
 * - 人体红外 SR602: GPIO34 (纯输入专用，高电平有效，RTC_GPIO4) ➔ 绑定 EXT1 唤醒
 * - 板载 LED2    : GPIO27 (工作期间高电平点亮指示)
 * ==============================================================================
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define PIN_BUTTON_SW3      GPIO_NUM_39 // 板载按键 SW3 (低电平有效)
#define PIN_PIR_SR602       GPIO_NUM_34 // 人体红外感应引脚 (有人时高电平)
#define PIN_LED             GPIO_NUM_27

static const char *TAG = "EXP2_EXT_WAKEUP";

// RTC 慢速内存统计变量
static RTC_DATA_ATTR int s_total_wakeups = 0;
static RTC_DATA_ATTR int s_button_wakeups = 0;
static RTC_DATA_ATTR int s_pir_wakeups = 0;

void app_main(void)
{
    s_total_wakeups++;

    // 初始化 LED2 指示灯
    gpio_reset_pin(PIN_LED);
    gpio_set_direction(PIN_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_LED, 1);

    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "   ⚡ Level 19 实验 2：EXT0 / EXT1 外部硬件引脚中断秒级唤醒 ");
    ESP_LOGI(TAG, "==========================================================");

    // 1. 识别具体的唤醒硬件来源
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            s_button_wakeups++;
            ESP_LOGI(TAG, "🔘 【唤醒来源】: EXT0 触发 ➔ \033[1;32m用户按下了 SW3 按键 (GPIO39)！\033[0m");
            break;

        case ESP_SLEEP_WAKEUP_EXT1: {
            s_pir_wakeups++;
            uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
            if (wakeup_pin_mask & (1ULL << PIN_PIR_SR602)) {
                ESP_LOGI(TAG, "🚶 【唤醒来源】: EXT1 触发 ➔ \033[1;33mSR602 人体红外感应到有人靠近 (GPIO34)！\033[0m");
            } else {
                ESP_LOGI(TAG, "⚡ 【唤醒来源】: EXT1 掩码触发 (引脚掩码: 0x%" PRIx64 ")", wakeup_pin_mask);
            }
            break;
        }

        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            ESP_LOGW(TAG, "🔌 【启动来源】: 首次外部通电冷启动 (Power-on Reset)！");
            break;
    }

    // 2. 打印 RTC 慢速内存统计数据
    ESP_LOGI(TAG, "📊 【RTC 唤醒历史统计】: 总唤醒: %d 次 | 🔘 按键触发: %d 次 | 🚶 红外感应: %d 次",
             s_total_wakeups, s_button_wakeups, s_pir_wakeups);

    // 模拟快速响应处理（如开屏显示或拍照，耗时 1.5 秒）
    ESP_LOGI(TAG, "⏳ 正在执行事件应急响应处理 (1.5 秒)...");
    vTaskDelay(pdMS_TO_TICKS(1500));

    // 3. 配置两大外部硬件唤醒源
    ESP_LOGI(TAG, "----------------------------------------------------------");
    ESP_LOGI(TAG, "⚙️ 正在配置外部休眠唤醒源:");
    ESP_LOGI(TAG, "   1. EXT0 ➔ 监听 SW3 按键 [GPIO%d]，低电平 (0) 时瞬间唤醒", PIN_BUTTON_SW3);
    ESP_LOGI(TAG, "   2. EXT1 ➔ 监听 SR602 红外 [GPIO%d]，高电平 (1) 时瞬间唤醒", PIN_PIR_SR602);
    ESP_LOGI(TAG, "----------------------------------------------------------");

    // ① 配置 EXT0：单引脚电平触发 (参数 0 表示低电平唤醒)
    esp_sleep_enable_ext0_wakeup(PIN_BUTTON_SW3, 0);

    // ② 配置 EXT1：多引脚位掩码触发 (ESP_EXT1_WAKEUP_ANY_HIGH 表示指定引脚中任意一个变高电平即唤醒)
    esp_sleep_enable_ext1_wakeup((1ULL << PIN_PIR_SR602), ESP_EXT1_WAKEUP_ANY_HIGH);

    ESP_LOGI(TAG, "💤 系统即将进入 Deep-sleep 深度睡眠模式 (5 μA 超低功耗)！");
    ESP_LOGI(TAG, "👉 请按下 SW3 按键 或 用手在 SR602 传感器前晃动，观察秒级唤醒复活！\n");

    gpio_set_level(PIN_LED, 0);

    // 4. 立即进入深度休眠
    esp_deep_sleep_start();
}
