/**
 * ==============================================================================
 * 🚀 ESP32 物联网实战闯关 —— 第 20 关：嵌入式软件工程与模块化分层架构
 * 📁 实验 3: 统一事件总线 (Event Bus) 发布-订阅多模块解耦工程
 * ==============================================================================
 * 
 * 📌 【架构说明】
 * - services/sys_event_bus.h / .c : 承载事件基底定义与总线分发服务
 * - main/app_main.c               : 模拟多个独立消费者（UI/MQTT/TF卡）向总线订阅与生产者广播
 * ==============================================================================
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "services/sys_event_bus.h"

static const char *TAG = "EXP3_MAIN";

/* ==============================================================================
 * 🖥️ 消费者 1：UI 屏幕渲染模块（订阅事件，刷新屏幕界面）
 * ============================================================================== */
static void ui_subscriber_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    if (base == SYS_EVENT_BASE && id == SYS_EVENT_TEMP_UPDATED) {
        sensor_event_payload_t *data = (sensor_event_payload_t *)event_data;
        ESP_LOGI(TAG, "🖥️ [UI 消费者] 收到总线广播 ➔ 刷新屏幕卡片: 温度 \033[36m%.1f °C\033[0m, 湿度 \033[36m%.1f %%\033[0m",
                 data->temperature, data->humidity);
    }
}

/* ==============================================================================
 * ☁️ 消费者 2：MQTT 物联网云端模块（订阅同一事件，打包上报）
 * ============================================================================== */
static void mqtt_subscriber_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    if (base == SYS_EVENT_BASE && id == SYS_EVENT_TEMP_UPDATED) {
        sensor_event_payload_t *data = (sensor_event_payload_t *)event_data;
        ESP_LOGI(TAG, "☁️ [MQTT 消费者] 收到总线广播 ➔ 打包 JSON 上传阿里云 IoT 平台...");
    }
}

/* ==============================================================================
 * 💾 消费者 3：TF 卡黑匣子日志模块（订阅同一事件，追加写本地文件）
 * ============================================================================== */
static void storage_subscriber_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    if (base == SYS_EVENT_BASE && id == SYS_EVENT_TEMP_UPDATED) {
        sensor_event_payload_t *data = (sensor_event_payload_t *)event_data;
        ESP_LOGI(TAG, "💾 [TF卡 消费者] 收到总线广播 ➔ 追加写入 /sdcard/sensor_log.csv (时间: %lu ms)", data->timestamp_ms);
    }
}

/* ==============================================================================
 * 📡 生产者：后台传感器采集任务（只管采集数据并向总线 post）
 * ============================================================================== */
static void sensor_producer_task(void *pvParameters)
{
    int count = 0;
    while (count < 3) {
        count++;
        vTaskDelay(pdMS_TO_TICKS(1500));

        sensor_event_payload_t payload = {
            .temperature = 25.0f + (float)count * 0.8f,
            .humidity = 60.0f + (float)count * 1.5f,
            .timestamp_ms = count * 1500,
        };

        ESP_LOGI(TAG, "\n📢 ----------------------------------------------------------");
        ESP_LOGI(TAG, "📢 [传感器采集生产者] 采样完毕，向全局事件总线广播新数据 (轮次: %d)...", count);
        ESP_LOGI(TAG, "📢 ----------------------------------------------------------");

        sys_event_bus_post(SYS_EVENT_TEMP_UPDATED, &payload, sizeof(payload));
    }

    ESP_LOGI(TAG, "\n🏆 统一事件总线发布-订阅多模块解耦工程验证 100% 成功！");
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "   🚀 Level 20 实验 3：统一事件总线 Event Bus 多文件实战   ");
    ESP_LOGI(TAG, "==========================================================");

    // 1. 启动全局事件总线服务
    sys_event_bus_init();

    // 2. 多个消费者各自独立向总线登记订阅
    esp_event_handler_instance_register(SYS_EVENT_BASE, SYS_EVENT_TEMP_UPDATED, ui_subscriber_handler, NULL, NULL);
    esp_event_handler_instance_register(SYS_EVENT_BASE, SYS_EVENT_TEMP_UPDATED, mqtt_subscriber_handler, NULL, NULL);
    esp_event_handler_instance_register(SYS_EVENT_BASE, SYS_EVENT_TEMP_UPDATED, storage_subscriber_handler, NULL, NULL);

    // 3. 启动后台传感器数据发布任务
    xTaskCreate(sensor_producer_task, "sensor_task", 3072, NULL, 5, NULL);
}
