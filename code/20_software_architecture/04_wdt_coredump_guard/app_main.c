/**
 * ==============================================================================
 * 🚀 ESP32 物联网实战闯关 —— 第 20 关：嵌入式软件工程与模块化分层架构
 * 📁 实验 4: 硬件看门狗 (TWDT) 守护与 Core Dump (Flash 存储) 崩溃诊断实战
 * ==============================================================================
 * 
 * 📌 【实验目标】
 * 1. 掌握服务层看门狗 (TWDT) 守护机制；
 * 2. 体验“正常喂狗 ➔ 模拟死循环卡死 ➔ 看门狗超时强制硬件级复位重启自愈”全流程；
 * 3. 掌握 Flash coredump 分区与 `espcoredump.py` 符号反解黑科技！
 * ==============================================================================
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "services/sys_guard_wdt.h"

static const char *TAG = "EXP4_GUARD_MAIN";

static void robust_worker_task(void *pvParameters)
{
    // 1. 将当前业务任务加入看门狗监控护航名单
    sys_guard_wdt_subscribe_current_task();
    ESP_LOGI(TAG, "🛡️ [业务任务] 成功加入看门狗监控护航名单！");

    int loop_count = 0;
    while (1) {
        loop_count++;
        ESP_LOGI(TAG, "🟢 [第 %d 轮正常工作] 执行传感器采集与数据上报...", loop_count);

        // 2. 正常工作时，按时向看门狗喂狗（重置 3000ms 倒计时）
        sys_guard_wdt_feed();
        ESP_LOGI(TAG, "   🍖 [及时喂狗] 倒计时已刷新重置！");

        vTaskDelay(pdMS_TO_TICKS(1000));

        // 3. 模拟在第 4 轮时发生死锁卡死（例如硬件 I2C 挂死不返回，不再喂狗）
        if (loop_count == 4) {
            ESP_LOGW(TAG, "\n💥 [模拟严重故障] 任务发生死锁卡死！停止喂狗 3 秒，观察看门狗硬件复位动作...");
            while (1) {
                // 故意死循环不喂狗，触发 TWDT 硬件超时复位！
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "   🚀 Level 20 实验 4：看门狗守护与 Core Dump 诊断实战     ");
    ESP_LOGI(TAG, "==========================================================");

    // 1. 初始化 3 秒看门狗守护服务 (超时触发内核 Panic 并刷写 Core Dump 到 Flash)
    sys_guard_wdt_init(3000);

    // 2. 启动业务工作任务
    xTaskCreate(robust_worker_task, "worker_task", 3072, NULL, 5, NULL);
}
