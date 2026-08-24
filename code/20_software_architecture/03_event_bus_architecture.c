/**
 * ==============================================================================
 * 🚀 ESP32 物联网实战闯关 —— 第 20 关：嵌入式软件工程与模块化分层架构
 * 📁 实验 3: 统一事件总线 (Event Bus) 发布-订阅模式多模块解耦架构 (终极大综合)
 * ==============================================================================
 * 
 * 📌 【实验目标】
 * 1. 掌握 ESP-IDF 原生事件循环（`esp_event_loop`）作为全局系统事件总线（Event Bus）的设计模式；
 * 2. 生产者（传感器采集任务）只负责向总线广播发布数据事件，完全不需要知道谁在接收；
 * 3. 消费者（UI 渲染模块、MQTT 云端同步模块）各自独立向总线订阅事件，实现真正的模块级解耦；
 * 4. 体会“增加一个新模块（如写TF卡日志）只需增加一个订阅者，原有模块 0 行代码修改”的终极扩展性！
 * ==============================================================================
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"

static const char *TAG = "EXP3_EVENT_BUS";

/* 1. 自定义系统事件基础标识 (Event Base) */
ESP_EVENT_DEFINE_BASE(SYSTEM_EVENT_BASE);

/* 2. 系统事件 ID 枚举 */
typedef enum {
    SYS_EVENT_TEMP_UPDATED = 0, // 气温更新事件
    SYS_EVENT_HUMI_UPDATED,     // 湿度更新事件
    SYS_EVENT_ALARM_TRIGGERED,  // 异常报警事件
} system_event_id_t;

/* 3. 伴随事件传递的载荷数据结构体 */
typedef struct {
    float temperature;
    float humidity;
    uint32_t timestamp_ms;
} sensor_data_event_t;

/* ==============================================================================
 * 🖥️ 消费者 1：UI 屏幕渲染模块（订阅事件，刷新屏幕界面）
 * ============================================================================== */
static void ui_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    if (base == SYSTEM_EVENT_BASE && id == SYS_EVENT_TEMP_UPDATED) {
        sensor_data_event_t *data = (sensor_data_event_t *)event_data;
        ESP_LOGI(TAG, "🖥️ [UI 渲染模块收到广播] ➔ 刷新屏幕仪表盘: 温度 \033[36m%.1f °C\033[0m, 湿度 \033[36m%.1f %%\033[0m",
                 data->temperature, data->humidity);
    }
}

/* ==============================================================================
 * ☁️ 消费者 2：MQTT 云端物联网模块（订阅同一事件，异步打包上报云平台）
 * ============================================================================== */
static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    if (base == SYSTEM_EVENT_BASE && id == SYS_EVENT_TEMP_UPDATED) {
        sensor_data_event_t *data = (sensor_data_event_t *)event_data;
        ESP_LOGI(TAG, "☁️ [MQTT 云端模块收到广播] ➔ 打包 JSON 遥测报文并推送至 MQTT Broker...");
    }
}

/* ==============================================================================
 * 💾 消费者 3：TF 卡黑匣子存储模块（订阅同一事件，追加写日志）
 * ============================================================================== */
static void storage_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    if (base == SYSTEM_EVENT_BASE && id == SYS_EVENT_TEMP_UPDATED) {
        sensor_data_event_t *data = (sensor_data_event_t *)event_data;
        ESP_LOGI(TAG, "💾 [TF卡 存储模块收到广播] ➔ 追加写入 /sdcard/sensor.csv (时间: %ld ms)", data->timestamp_ms);
    }
}

/* ==============================================================================
 * 📡 生产者：后台传感器采集任务（只负责采集与向全局事件总线广播）
 * ============================================================================== */
static void sensor_producer_task(void *pvParameters)
{
    int count = 0;
    while (count < 4) {
        count++;
        vTaskDelay(pdMS_TO_TICKS(1500));

        sensor_data_event_t evt_payload = {
            .temperature = 25.0f + (float)count * 0.8f,
            .humidity = 60.0f + (float)count * 1.2f,
            .timestamp_ms = count * 1500,
        };

        ESP_LOGI(TAG, "\n📢 ----------------------------------------------------------");
        ESP_LOGI(TAG, "📢 [传感器采集生产者] 采样完毕，向全局事件总线广播新数据 (轮次: %d)...", count);
        ESP_LOGI(TAG, "📢 ----------------------------------------------------------");

        // 向全局系统事件总线发布事件（系统会自动分发给所有订阅了此事件的消费者）
        esp_event_post(SYSTEM_EVENT_BASE, SYS_EVENT_TEMP_UPDATED, 
                       &evt_payload, sizeof(evt_payload), portMAX_DELAY);
    }

    ESP_LOGI(TAG, "\n🏆 统一事件总线发布-订阅解耦多模块系统验证 100% 成功！");
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "   🚀 Level 20 实验 3：统一事件总线 (Event Bus) 架构实战   ");
    ESP_LOGI(TAG, "==========================================================");

    // 1. 初始化系统默认事件循环总线 (Event Loop)
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 2. 各业务消费者独立向总线注册监听感应器
    ESP_ERROR_CHECK(esp_event_handler_instance_register(SYSTEM_EVENT_BASE, SYS_EVENT_TEMP_UPDATED, 
                                                       &ui_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(SYSTEM_EVENT_BASE, SYS_EVENT_TEMP_UPDATED, 
                                                       &mqtt_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(SYSTEM_EVENT_BASE, SYS_EVENT_TEMP_UPDATED, 
                                                       &storage_event_handler, NULL, NULL));

    // 3. 启动后台数据采集生产者任务
    xTaskCreate(sensor_producer_task, "sensor_task", 3072, NULL, 5, NULL);
}
