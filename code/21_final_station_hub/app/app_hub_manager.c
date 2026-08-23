#include "app_hub_manager.h"
#include "bsp_led_button.h"
#include "bsp_sensors.h"
#include "bsp_ws2812.h"
#include "srv_event_bus.h"
#include "srv_wifi_sntp.h"
#include "srv_mqtt.h"
#include "srv_ble.h"
#include "srv_sentry_power.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "APP_HUB_MGR";

static void on_hub_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (id == HUB_EVENT_CMD_TOGGLE_LED) {
        bsp_led_toggle();
        ESP_LOGI(TAG, "💡 收到控制指令 ➔ 翻转板载 LED2 (当前: %s)", bsp_led_get() ? "ON" : "OFF");
        if (bsp_led_get()) {
            bsp_ws2812_set_color(0, 25, 0); // 绿光
        } else {
            bsp_ws2812_clear();
        }
    }
}

static void sensor_telemetry_task(void *arg)
{
    hub_sensors_data_t data;
    char time_str[32];
    int count = 0;

    while (1) {
        srv_sentry_feed_dog(); // 喂狗

        // 1. 采集数据
        bsp_sensors_read_all(&data);
        srv_wifi_get_time_str(time_str, sizeof(time_str));

        // 2. 打印控制台中枢面板
        ESP_LOGI(TAG, "==================================================");
        ESP_LOGI(TAG, "⏰ 【智能气象中控台】 时间: %s", time_str);
        ESP_LOGI(TAG, "🌡️ 气温: %.1f°C | 湿度: %.1f%% | 测距: %.1fcm | PIR: %s",
                 data.temperature, data.humidity, data.distance_cm,
                 data.pir_motion ? "有人靠近!" : "静止无人");
        ESP_LOGI(TAG, "💻 系统内存: %lu KB | Wi-Fi: %s | MQTT: %s",
                 data.free_heap / 1024,
                 srv_wifi_is_connected() ? "在线" : "连接中...",
                 srv_mqtt_is_connected() ? "已连接" : "离线");
        ESP_LOGI(TAG, "==================================================");

        // 3. 构建 JSON 遥测报文并上传 MQTT 云端
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "time", time_str);
        cJSON_AddNumberToObject(root, "temp", (int)(data.temperature * 10) / 10.0);
        cJSON_AddNumberToObject(root, "humi", (int)(data.humidity * 10) / 10.0);
        cJSON_AddNumberToObject(root, "dist", (int)(data.distance_cm * 10) / 10.0);
        cJSON_AddBoolToObject(root, "pir", data.pir_motion);
        cJSON_AddNumberToObject(root, "heap_kb", data.free_heap / 1024);

        char *json_str = cJSON_PrintUnformatted(root);
        if (srv_mqtt_is_connected()) {
            srv_mqtt_publish_telemetry(json_str);
            ESP_LOGI(TAG, "📤 [遥测上报云端] %s", json_str);
        }
        free(json_str);
        cJSON_Delete(root);

        // 4. 按键检测
        if (bsp_button_is_pressed()) {
            srv_event_bus_post(HUB_EVENT_CMD_TOGGLE_LED, NULL, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(3000));
        count++;
    }
}

esp_err_t app_hub_manager_start(void)
{
    // 注册事件总线监听
    esp_event_handler_instance_register(HUB_EVENT_BASE, ESP_EVENT_ANY_ID, on_hub_event, NULL, NULL);

    // 创建核心中枢任务
    xTaskCreatePinnedToCore(sensor_telemetry_task, "hub_telemetry", 4096, NULL, 5, NULL, 0);

    ESP_LOGI(TAG, "🚀 超级中控管理应用 (App Hub Manager) 已启动运行！");
    return ESP_OK;
}
