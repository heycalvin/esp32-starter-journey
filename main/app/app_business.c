#include "app_business.h"
#include "bsp_led.h"
#include "bsp_button.h"
#include "bsp_sensor.h"
#include "sys_event_bus.h"
#include "sys_fsm.h"
#include "sys_guard_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "APP_BIZ";

/* 传感器数据事件监听回调 */
static void on_sensor_updated(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    bsp_sensor_data_t *data = (bsp_sensor_data_t *)event_data;
    ESP_LOGI(TAG, "📊 [UI看板/云端消费事件] 气温: %.1f°C | 湿度: %.1f%% | 剩余堆内存: %lu KB",
             data->temperature, data->humidity, data->free_heap / 1024);
}

/* 按键事件监听回调 */
static void on_button_pressed(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "🔘 [按键业务响应] 用户按下 SW3，翻转 LED 并通知状态机");
    bsp_led_set(BSP_LED_TOGGLE);
    sys_fsm_handle_event(SYS_EVENT_ALARM_TRIGGERED);
}

/* 传感器采集工作线程 (带看门狗守护) */
static void sensor_worker_task(void *arg)
{
    sys_guard_wdt_subscribe_current_task();
    bsp_sensor_data_t sensor_data;

    while (1) {
        // 喂狗
        sys_guard_wdt_feed();

        // 采集数据并通过事件总线广播
        if (bsp_sensor_read(&sensor_data) == ESP_OK) {
            sys_event_bus_post(SYS_EVENT_SENSOR_UPDATED, &sensor_data, sizeof(sensor_data));
        }

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/* 按键扫描工作线程 */
static void button_worker_task(void *arg)
{
    while (1) {
        if (bsp_button_is_pressed()) {
            sys_event_bus_post(SYS_EVENT_BUTTON_PRESSED, NULL, 0);
            while (bsp_button_is_pressed()) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t app_business_start(void)
{
    // 1. 订阅事件总线
    esp_event_handler_instance_register(SYS_EVENT_BASE, SYS_EVENT_SENSOR_UPDATED, on_sensor_updated, NULL, NULL);
    esp_event_handler_instance_register(SYS_EVENT_BASE, SYS_EVENT_BUTTON_PRESSED, on_button_pressed, NULL, NULL);

    // 2. 创建并发业务任务
    xTaskCreate(sensor_worker_task, "sensor_worker", 3072, NULL, 5, NULL);
    xTaskCreate(button_worker_task, "btn_worker", 2048, NULL, 4, NULL);

    ESP_LOGI(TAG, "🚀 业务应用层 (Application Layer) 调度运行中...");
    return ESP_OK;
}
