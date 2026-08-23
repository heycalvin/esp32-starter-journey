/**
 * 🌟 ESP32 物联网实战 —— 第 21 关：桌面多功能智能气象站与物联网超级中控台 (毕业设计全栈总成)
 * 
 * 🎯 终极全栈技能融合与企业级分层架构：
 *    - 【Level 1: BSP 驱动层】: 板载 LED/按键 (GPIO27/39), 传感器融合 (DHT11/NTC/超声波/PIR), WS2812 幻彩 RGB
 *    - 【Level 2: 服务中间件】: 全局事件总线, Wi-Fi+SNTP 自动授时, MQTT 遥测与双向控制, BLE 蓝牙, 电源看门狗
 *    - 【Level 3: 业务管理层】: app_hub_manager 双核高并发调度、控制指令闭环与云端上报
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

// 引入全栈分层组件
#include "bsp_board.h"
#include "bsp_led_button.h"
#include "bsp_sensors.h"
#include "bsp_ws2812.h"
#include "srv_event_bus.h"
#include "srv_wifi_sntp.h"
#include "srv_mqtt.h"
#include "srv_ble.h"
#include "srv_sentry_power.h"
#include "app_hub_manager.h"

static const char *TAG = "SUPER_HUB_MAIN";

#define WIFI_SSID       "ESP32_SMART_HUB"
#define WIFI_PASS       "12345678"
#define MQTT_BROKER_URI "mqtt://broker.emqx.io:1883"

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🏆 关卡 21 终极大结局：桌面智能中控台总成启动  ");
    ESP_LOGI(TAG, "==================================================");

    // 0. NVS 初始化
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 1. 初始化 Level 1: BSP 板级硬件外设
    ESP_LOGI(TAG, "📦 [1/3] 初始化 BSP 板级驱动 (LED/Key/Sensors/WS2812)...");
    ESP_ERROR_CHECK(bsp_led_button_init());
    ESP_ERROR_CHECK(bsp_sensors_init());
    bsp_ws2812_init();

    // 2. 初始化 Level 2: 中间件与系统服务
    ESP_LOGI(TAG, "⚙️ [2/3] 初始化系统服务 (Event Bus / Wi-Fi / MQTT / BLE / Watchdog)...");
    ESP_ERROR_CHECK(srv_event_bus_init());
    ESP_ERROR_CHECK(srv_sentry_power_init());
    srv_ble_start("ESP32-Super-Hub");
    srv_wifi_sntp_start(WIFI_SSID, WIFI_PASS);
    srv_mqtt_start(MQTT_BROKER_URI);

    // 3. 启动 Level 3: 业务管理应用层
    ESP_LOGI(TAG, "🚀 [3/3] 启动超级中控业务管理中枢...");
    ESP_ERROR_CHECK(app_hub_manager_start());

    ESP_LOGI(TAG, "🎉 [系统全栈就绪] 桌面多功能智能气象站与超级中控台正在平稳运行！");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
