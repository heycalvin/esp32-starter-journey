#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bsp/bsp_led.h"
#include "bsp/bsp_button.h"
#include "bsp/bsp_sensor.h"
#include "bsp/bsp_sdcard.h"
#include "bsp/bsp_display.h"
#include "bsp/bsp_lvgl_port.h"
#include "services/sys_event_bus.h"
#include "services/sys_fsm.h"
#include "services/sys_guard_wdt.h"
#include "services/sys_ota.h"
#include "services/net_manager.h"
#include "services/ble_manager.h"
#include "services/file_reader.h"
#include "services/sys_font_manager.h"
#include "services/web_server.h"
#include "ui/ui_hub.h"
#include "app/app_business.h"

static const char *TAG = "FINAL_SMART_HUB";

void app_main(void)
{
    ESP_LOGI(TAG, "========================================================================");
    ESP_LOGI(TAG, "   🏆 关卡 21 毕业设计：桌面多功能智能气象站与物联网超级中控台全栈总成   ");
    ESP_LOGI(TAG, "========================================================================");

    // 1. 初始化 BSP 板级驱动抽象层
    bsp_led_init();
    bsp_button_init();
    bsp_sensor_init();
    bsp_sdcard_init();

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_touch_handle_t touch_handle = NULL;
    bsp_display_init(&io_handle, &panel_handle);
    bsp_touch_init(&touch_handle);
    bsp_lvgl_port_init(io_handle, panel_handle, touch_handle);

    // 2. 初始化系统服务层 (Services)
    sys_event_bus_init();
    sys_fsm_init();
    sys_guard_wdt_init(6000); // 6秒看门狗守护
    sys_ota_init();
    file_reader_init();
    sys_font_manager_init();
    net_manager_init("ESP32_WIFI", "12345678");
    ble_manager_init("ESP32-Smart-Hub");
    web_server_init();

    // 3. 初始化 UI 交互层
    ui_hub_init();

    // 4. 启动业务逻辑调度中枢
    app_business_start();

    sys_fsm_handle_event(HUB_FSM_EVT_INIT_DONE, 0);
    ESP_LOGI(TAG, "🎉 [系统就绪] 桌面智能中控台全栈全功能系统 100% 流水线装配成功！");
}
