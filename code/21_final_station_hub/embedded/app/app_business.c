#include "app_business.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bsp_sensor.h"
#include "bsp_button.h"
#include "bsp_led.h"
#include "sys_event_bus.h"
#include "sys_guard_wdt.h"
#include "net_manager.h"
#include "file_reader.h"
#include "ui_hub.h"

static const char *TAG = "APP_BIZ";

/* 后台传感器采样与遥测中枢任务 (运行在 CPU Core 0) */
static void sensor_telemetry_task(void *pvParameters)
{
    ESP_LOGI(TAG, "📡 [业务中枢] 多传感器融合与遥测上报任务启动 (Core 0)...");
    sys_guard_wdt_subscribe_current_task();

    bsp_sensor_data_t sensor_data;
    char time_str[32] = "16:00:00";
    char date_str[64] = "2026年08月25日 星期二";
    char uptime_str[32] = "00:00:00 (0s)";
    char ip_str[32] = "192.168.4.1";
    int cycle_cnt = 0;

    while (1) {
        cycle_cnt++;

        // 1. 采集全套传感器与时钟/网络状态
        bsp_sensor_read_all(&sensor_data);
        net_manager_get_time_str(time_str, sizeof(time_str));
        net_manager_get_date_str(date_str, sizeof(date_str));
        net_manager_get_uptime_str(uptime_str, sizeof(uptime_str));
        net_manager_get_ip_str(ip_str, sizeof(ip_str));

        // 2. 更新 LVGL 界面 (首页气象时钟看板、IoT仪表盘、系统运行时间)
        ui_hub_update_time_and_date(time_str, date_str);
        ui_hub_update_sensor_data(&sensor_data);
        ui_hub_update_system_status(uptime_str, ip_str, sensor_data.free_heap_bytes, sensor_data.free_psram_bytes);

        if (!net_manager_is_provisioning()) {
            ui_hub_update_weather_full("深圳市 · 晴朗", "晴朗舒适 · 适宜阅读", sensor_data.ntc_temperature, sensor_data.dht_humidity);
        }

        // 3. 广播事件到总线
        sys_event_bus_post(HUB_EVT_SENSOR_UPDATED, &sensor_data, sizeof(sensor_data));

        // 4. 追加写入 TF 卡传感器黑匣子
        file_reader_append_sensor_log(time_str, sensor_data.ntc_temperature, sensor_data.dht_humidity, sensor_data.ultrasonic_dist_cm);

        // 5. 每 3 秒上传 MQTT 遥测 JSON
        if (cycle_cnt % 3 == 0) {
            char json_buf[256];
            snprintf(json_buf, sizeof(json_buf),
                     "{\"time\":\"%s\",\"temp\":%.1f,\"humi\":%.1f,\"dist\":%.1f,\"pir\":%d,\"heap\":%lu}",
                     time_str, sensor_data.ntc_temperature, sensor_data.dht_humidity,
                     sensor_data.ultrasonic_dist_cm, sensor_data.pir_motion_detected ? 1 : 0,
                     (unsigned long)(sensor_data.free_heap_bytes / 1024));
            net_manager_publish_telemetry(json_buf);
        }

        // 6. 按键长短按智能检测响应 (短按开关灯，长按3秒重置配网)
        if (bsp_button_is_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(30));
            if (bsp_button_is_pressed()) {
                int press_ms = 0;
                while (bsp_button_is_pressed() && press_ms < 3200) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                    press_ms += 50;
                }

                if (press_ms >= 3000) {
                    ESP_LOGW(TAG, "🔘 [长按 3 秒触发] 正在重置 Wi-Fi 凭据并重启回 AP 配网模式...");
                    net_manager_reset_credentials();
                } else {
                    bsp_led_toggle();
                    ESP_LOGI(TAG, "🔘 [短按触发] 用户按下板载 SW3，翻转 LED2 状态 ➔ %s", bsp_led_get_state() ? "ON" : "OFF");
                }
            }
        }

        // 7. 喂看门狗
        sys_guard_wdt_feed();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t app_business_start(void)
{
    // 创建 Core 0 上的传感器融合与网络遥测调度任务
    xTaskCreatePinnedToCore(sensor_telemetry_task, "sensor_task", 4096, NULL, 4, NULL, 0);
    ESP_LOGI(TAG, "🚀 [业务层] 智能中控双核多任务调度系统正式启动！");
    return ESP_OK;
}
