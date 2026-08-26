#include "app_business.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bsp_sensor.h"
#include "bsp_button.h"
#include "bsp_sdcard.h"
#include "sys_event_bus.h"
#include "sys_guard_wdt.h"
#include "net_manager.h"
#include "file_reader.h"
#include "ui_hub.h"
#include "ui_pomodoro.h"

static const char *TAG = "APP_BIZ";

#define BUTTON_SAMPLE_MS       20
#define BUTTON_DEBOUNCE_SAMPLES 2
#define BUTTON_LONG_PRESS_MS   3000

/* SW3 导航任务：按键不再占用传感器遥测任务的执行周期。 */
static void button_navigation_task(void *pvParameters)
{
    bool stable_pressed = false;
    uint8_t debounce_samples = 0;
    uint32_t held_ms = 0;
    bool long_press_sent = false;

    ESP_LOGI(TAG, "🔘 [按键] SW3 导航任务启动：短按导航，长按 %d 秒重置配网", BUTTON_LONG_PRESS_MS / 1000);

    while (1) {
        bool raw_pressed = bsp_button_is_pressed();

        if (raw_pressed != stable_pressed) {
            debounce_samples++;
            if (debounce_samples >= BUTTON_DEBOUNCE_SAMPLES) {
                bool was_pressed = stable_pressed;
                stable_pressed = raw_pressed;
                debounce_samples = 0;

                if (stable_pressed) {
                    held_ms = 0;
                    long_press_sent = false;
                } else if (was_pressed) {
                    if (!long_press_sent && held_ms >= 40) {
                        ui_hub_handle_sw3_short_press();
                    }
                    held_ms = 0;
                    long_press_sent = false;
                }
            }
        } else {
            debounce_samples = 0;
        }

        if (stable_pressed) {
            if (held_ms < BUTTON_LONG_PRESS_MS) held_ms += BUTTON_SAMPLE_MS;
            if (held_ms >= BUTTON_LONG_PRESS_MS && !long_press_sent) {
                long_press_sent = true;
                ESP_LOGW(TAG, "🔘 [长按 3 秒触发] 正在重置 Wi-Fi 凭据并重启回 AP 配网模式...");
                net_manager_reset_credentials();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(BUTTON_SAMPLE_MS));
    }
}

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

        // 0. TF 卡热插拔动态侦测重试 (未挂载时每 4 秒自动尝试侦测挂载)
        if (!bsp_sdcard_is_mounted() && (cycle_cnt % 4 == 0)) {
            bsp_sdcard_init();
        }

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

        // 更新番茄钟/大时钟 Tab（解析 HH:MM:SS 格式）
        {
            int h = 0, m = 0, s = 0;
            sscanf(time_str, "%d:%d:%d", &h, &m, &s);
            ui_pomodoro_tick(h, m, s);
            ui_pomodoro_update_date(date_str);
        }

        char loc_str[64] = {0};
        char w_desc[64] = {0};
        net_manager_get_location_str(loc_str, sizeof(loc_str));
        net_manager_get_weather_str(w_desc, sizeof(w_desc));

        if (!net_manager_is_provisioning()) {
            ui_hub_update_weather_full(loc_str, w_desc, sensor_data.ntc_temperature, sensor_data.dht_humidity);
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

        // 6. 喂看门狗
        sys_guard_wdt_feed();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t app_business_start(void)
{
    // 独立按键任务保证 SW3 导航不被传感器采样阻塞
    xTaskCreatePinnedToCore(button_navigation_task, "button_task", 3072, NULL, 5, NULL, 1);

    // 创建 Core 0 上的传感器融合与网络遥测调度任务
    xTaskCreatePinnedToCore(sensor_telemetry_task, "sensor_task", 4096, NULL, 4, NULL, 0);
    ESP_LOGI(TAG, "🚀 [业务层] 智能中控双核多任务调度系统正式启动！");
    return ESP_OK;
}
