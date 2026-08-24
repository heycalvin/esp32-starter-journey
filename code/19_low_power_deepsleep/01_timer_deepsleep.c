/**
 * ==============================================================================
 * 🚀 ESP32 物联网实战闯关 —— 第 19 关：低功耗电源管理与 Deep-sleep 深度睡眠
 * 📁 实验 1: 定时器深度睡眠 (Timer Wakeup) 与 RTC 慢速内存掉电数据保持
 * ==============================================================================
 * 
 * 📌 【实验目标】
 * 1. 掌握 ESP32 深度睡眠（Deep-sleep）工作机制与 5μA 微安级超低功耗特性；
 * 2. 掌握 RTC 慢速内存（RTC Slow Memory）原理，使用 RTC_DATA_ATTR 实现掉电数据不丢失；
 * 3. 使用 esp_sleep_get_wakeup_cause() 精准识别是“冷启动上电”还是“深度睡眠睡醒”；
 * 4. 配置微秒级定时器唤醒源（esp_sleep_enable_timer_wakeup）并进入秒级周期睡眠！
 * 
 * 📌 【硬件引脚说明】
 * - 板载 LED2: GPIO27 (工作期间高电平点亮，进入 Deep-sleep 期间自动断电熄灭)
 * ==============================================================================
 */

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_PIN         GPIO_NUM_27
#define SLEEP_TIME_SEC  5       // 每次深度睡眠 5 秒
static const char *TAG = "EXP1_TIMER_SLEEP";

// ⚡ 核心黑科技：保存在 8KB RTC 慢速内存中的掉电保持变量！
// 普通变量在 Deep-sleep 期间因 CPU 断电会彻底丢失，但带有 RTC_DATA_ATTR 的变量能完好保存！
static RTC_DATA_ATTR int s_boot_count = 0;
static RTC_DATA_ATTR time_t s_last_wake_time = 0;

void app_main(void)
{
    // 1. 累计开机/唤醒总次数 (存放在 RTC 慢速内存中)
    s_boot_count++;

    // 2. 初始化 LED2 指示灯
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 1); // 点亮 LED 表明系统处于活跃运行态 (Active)

    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "   ⚡ Level 19 实验 1：Timer 定时器深度睡眠与 RTC 数据保持 ");
    ESP_LOGI(TAG, "==========================================================");

    // 3. 诊断本次启动原因：判断是插电冷启动还是睡醒复活
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "⏰ 【唤醒来源】: 定时器闹钟响了 (ESP_SLEEP_WAKEUP_TIMER)！芯片从深度睡眠中苏醒！");
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            ESP_LOGW(TAG, "🔌 【启动来源】: 首次外部通电冷启动 (Power-on Reset)！");
            break;
    }

    ESP_LOGI(TAG, "📊 【RTC 内存数据】: 累计苏醒运行次数: 第 \033[1;32m%d\033[0m 次", s_boot_count);
    ESP_LOGI(TAG, "💡 【工作状态】: LED2 已点亮，CPU 双核 @ 240MHz 全速工作 (活跃电流 ~150mA)");

    // 模拟执行 2 秒的核心采集/计算业务
    ESP_LOGI(TAG, "⏳ 正在模拟执行业务数据采集与处理 (耗时 2 秒)...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 4. 准备进入微安级深度睡眠
    ESP_LOGI(TAG, "----------------------------------------------------------");
    ESP_LOGI(TAG, "💤 业务处理完毕！正在配置 %d 秒后定时唤醒...", SLEEP_TIME_SEC);
    ESP_LOGI(TAG, "🔌 即将切断 CPU/RAM/外设主电源，整机功耗降至 \033[1;36m~5 μA (微安)\033[0m！");
    ESP_LOGI(TAG, "----------------------------------------------------------\n");

    // 熄灭 LED 表明即将切断主电源
    gpio_set_level(LED_PIN, 0);

    // 5. 启用 Timer 唤醒源 (入参单位为微秒 μs: 1 秒 = 1,000,000 微秒)
    esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_TIME_SEC * 1000000ULL);

    // 6. 核心指令：芯片立即断电进入 Deep-sleep！
    // 💡 注意：执行此函数后芯片主电源关闭，此行之后的任何代码都不会被执行！
    // 睡醒后芯片会像复位重启一样，重新从 app_main() 的第一行开始执行！
    esp_deep_sleep_start();
}
