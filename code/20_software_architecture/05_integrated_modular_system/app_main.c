/**
 * 🌟 ESP32 物联网实战 —— 第 20 关：嵌入式软件工程化与模块化分层架构 (Multi-file Architecture)
 * 
 * 🎯 软件工程化标准三层架构：
 *    - 【Level 1: BSP 板级驱动层】: bsp_led, bsp_button, bsp_sensor
 *    - 【Level 2: 中间件与服务层】: sys_event_bus (统一事件总线), sys_fsm (状态机), sys_guard_wdt (看门狗)
 *    - 【Level 3: 业务应用与交互层】: app_business (多任务异步解耦流转)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

// 引入清晰分层头文件 (IDE 与构建系统均可秒级解析)
#include "bsp/bsp_led.h"
#include "bsp/bsp_button.h"
#include "bsp/bsp_sensor.h"
#include "services/sys_event_bus.h"
#include "services/sys_fsm.h"
#include "services/sys_guard_wdt.h"
#include "app/app_business.h"

static const char *TAG = "MAIN_SYSTEM";

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🏗️ 关卡 20：企业级多文件模块化分层工程启动       ");
    ESP_LOGI(TAG, "==================================================");

    // 0. NVS 初始化
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 1. 初始化 Level 1: BSP 板级硬件驱动层
    ESP_LOGI(TAG, "📦 [1/3] 初始化 BSP 硬件驱动抽象层...");
    ESP_ERROR_CHECK(bsp_led_init());
    ESP_ERROR_CHECK(bsp_button_init());
    ESP_ERROR_CHECK(bsp_sensor_init());

    // 2. 初始化 Level 2: 中间件与系统服务层
    ESP_LOGI(TAG, "⚙️ [2/3] 初始化中间件与系统服务层 (Event Bus / FSM / WDT)...");
    ESP_ERROR_CHECK(sys_event_bus_init());
    ESP_ERROR_CHECK(sys_fsm_init());
    ESP_ERROR_CHECK(sys_guard_wdt_init(5000)); // 5秒任务看门狗

    // 3. 启动 Level 3: 业务逻辑应用层
    ESP_LOGI(TAG, "🚀 [3/3] 启动业务逻辑任务调度...");
    ESP_ERROR_CHECK(app_business_start());

    ESP_LOGI(TAG, "🎉 [系统就绪] 所有模块解耦就绪，正在平稳运行！");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
