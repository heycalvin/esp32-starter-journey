/**
 * 🌟 ESP32 物联网实战 —— 第 14 关 实验 2：自定义 BLE 广播包与动态温湿度信标 (主包直出 VS 62字节双包扩容)
 * 
 * 🎯 学习目标与背景铺垫：
 *    1. 【31 字节预算意识】：理解为什么主广播包被超长设备名和多个服务 UUID 占满后，必须借助扫描响应包扩容。
 *    2. 【模式 0：主包直载模式】：直接在主广播包携带厂商温湿度数据，所有手机免连接在扫描列表秒级直出！
 *    3. 【模式 1：双包 62 字节极限扩容模式】：主包放超长设备名，响应包放厂商数据与硬件序列号，容量翻倍！
 *    4. 【动态广播热更新 (Dynamic Advertising)】：
 *       创建后台定时任务，每隔 3 秒模拟采集最新温湿度，并调用 `esp_ble_gap_config_adv_data` 实时刷新空中广播！
 * 
 * 📌 验证方法：
 *    手机打开 LightBlue App 或 nRF Connect，无需点击 Connect 连接，
 *    直接查看广播详情中的 Manufacturer Specific Data，即可看到温度数据每隔 3 秒实时跳变！
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

/* 蓝牙核心头文件 */
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"

static const char *TAG = "EXP2_CUSTOM_ADV";

/* ==============================================================================
 * 🎯 广播载荷分配策略选择开关 (0 或 1)
 * 
 * 模式 0【主包直载模式 (Primary ADV Direct)】：
 *    - 策略：主包放精简设备名 + 厂商传感器数据（≤31 字节）。
 *    - 优势：所有手机无论主动/被动扫描，扫描列表中 100% 秒级直出 Manufacturer Data！
 * 
 * 模式 1【双包协同扩容模式 (Scan Response Expansion)】：
 *    - 策略：主包放超长设备名 + 16位服务 UUID 列表（撑满 31 字节）；
 *           扫描响应包放厂商温湿度 + 额外设备序列号（总容量达 62 字节，突破极限！）。
 *    - 手机查看：需使用支持主动扫描的 App（如 nRF Connect 或在 LightBlue 中下拉刷新触发主动扫描）。
 * ============================================================================== */
#define ADV_ALLOCATION_MODE   0   // 👉 修改此宏为 0 或 1 即可自由切换两种架构！

#if ADV_ALLOCATION_MODE == 0
    #define DEVICE_NAME "ESP32-Beacon"
#else
    #define DEVICE_NAME "ESP32-Smart-Air-Sensor-Pro"  // 超长设备名 (占 28 字节)
#endif

/* 自定义厂商数据 Payload (模拟温湿度与电量数据) */
// 格式：[厂商ID低字节, 厂商ID高字节, 温度(℃), 湿度(%), 电池电量(%)]
static uint8_t s_sensor_payload[] = {
    0xE5, 0x02,  // 乐鑫 Espressif 官方厂商 ID (0x02E5)
    25,          // 初始温度: 25 ℃
    60,          // 初始湿度: 60 %
    98           // 电池电量: 98 %
};

#if ADV_ALLOCATION_MODE == 1
/* 额外设备序列号（放在扫描响应包中进一步扩容） */
static uint8_t s_serial_number_payload[] = {
    0xE5, 0x02, 25, 60, 98, 'S', 'N', '2', '0', '2', '6', 'X', '1'
};
#endif

/* 广播物理参数：每 50ms 广播一次 */
static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x40, // 40ms
    .adv_int_max        = 0x80, // 80ms
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

#if ADV_ALLOCATION_MODE == 0
/* 模式 0 广播配置：传感器数据直接挂载在主包中 */
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,  // "ESP32-Beacon"
    .include_txpower     = true,
    .flag                = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
    .p_manufacturer_data = s_sensor_payload,
    .manufacturer_len    = sizeof(s_sensor_payload),
};

static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp        = true,
    .include_name        = true,
};
#else
/* 模式 1 广播配置：主包放超长设备名与标志（撑满 31 字节），扫描响应包放传感器与序列号扩容 */
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,  // 超长设备名占满主包
    .include_txpower     = true,
    .flag                = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp        = true,  // ⭐️ 扫描响应包承接大数据
    .p_manufacturer_data = s_serial_number_payload,
    .manufacturer_len    = sizeof(s_serial_number_payload),
};
#endif

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        // 主广播包配置完成 ➔ 继续配置扫描响应包
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            esp_ble_gap_config_adv_data(&scan_rsp_data);
            break;

        // 扫描响应包配置完成 ➔ 双包全部就绪，开启广播
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            ESP_LOGI(TAG, "📡 主广播包 + 扫描响应包双包就绪，开启 BLE 广播...");
            esp_ble_gap_start_advertising(&adv_params);
            break;

        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "🎉 [BLE 广播成功启动] 模式: %s, 设备名: \033[32m%s\033[0m",
                         (ADV_ALLOCATION_MODE == 0) ? "主包直载模式" : "双包62字节扩容模式", DEVICE_NAME);
            } else {
                ESP_LOGE(TAG, "❌ 广播启动失败, 错误码: %d", param->adv_start_cmpl.status);
            }
            break;

        default:
            break;
    }
}

/**
 * 📌 后台任务：每隔 3 秒模拟环境温湿度变化，动态热更新空中广播！
 */
static void sensor_simulator_task(void *pvParameters)
{
    uint8_t temp = 25;
    uint8_t humi = 60;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(3000)); // 每 3 秒更新一次

        temp++;
        if (temp > 30) temp = 24;
        humi = (humi >= 70) ? 55 : (humi + 1);

        s_sensor_payload[2] = temp; // 温度
        s_sensor_payload[3] = humi; // 湿度

        ESP_LOGI(TAG, "🌡️ 传感器数据刷新 ➔ 温度: %d ℃, 湿度: %d %% ➔ 实时热更新广播包！", temp, humi);

#if ADV_ALLOCATION_MODE == 0
        // 模式 0：热更新主广播包
        esp_ble_gap_config_adv_data(&adv_data);
#else
        // 模式 1：热更新扫描响应包
        s_serial_number_payload[2] = temp;
        s_serial_number_payload[3] = humi;
        esp_ble_gap_config_adv_data(&scan_rsp_data);
#endif
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 14 实验 2：自定义广播包与动态温湿度信标  ");
    ESP_LOGI(TAG, "==================================================");

    /* 1. 初始化 NVS Flash */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 2. 释放经典蓝牙内存 */
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    /* 3. 初始化硬件 BT 控制器并使能 BLE */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    /* 4. 初始化并启动 Bluedroid 协议栈 */
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    /* 5. 注册 GAP 回调并设置设备名 */
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_set_device_name(DEVICE_NAME));

    /* 6. 提交配置主广播包 */
    ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&adv_data));

    /* 7. 创建后台动态广播更新任务 */
    xTaskCreate(sensor_simulator_task, "sensor_sim", 2048, NULL, 5, NULL);
}
