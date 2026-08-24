/**
 * 🌟 ESP32 物联网实战 —— 第 14 关 实验 1：BLE 低功耗蓝牙广播与手机扫描发现 (BLE Beacon)
 * 
 * 🎯 学习目标与背景铺垫：
 *    1. 【搞懂广播的物理本质】：低功耗蓝牙（BLE）在未连接状态下，通过 37、38、39 三个专用广播信道，
 *       以设定的时间间隔（如 20ms~40ms）向空中发射无线电脉冲信号（广播包 Advertising Packet）。
 *    2. 【掌握五步初始化流水线】：
 *       ① NVS Flash 初始化（蓝牙底层存储配置与配对密钥必需）；
 *       ② 释放经典蓝牙内存（esp_bt_controller_mem_release，省出约 30KB 内存）；
 *       ③ 初始化底层硬件 BT 控制器并使能 BLE 模式；
 *       ④ 初始化并启用 Bluedroid 蓝牙协议栈；
 *       ⑤ 注册 GAP 回调函数，配置广播名与广播参数，开启广播！
 *    3. 【真机验证】：手机下载 LightBlue App，点击扫描，即可秒级搜到 "ESP32-Journey-Beacon"！
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

/* ESP-IDF 官方 Bluedroid 蓝牙协议栈核心头文件 */
#include "esp_bt.h"           // 硬件 BT 控制器接口与内存管理
#include "esp_gap_ble_api.h"   // GAP（通用访问协议）API：管理广播、扫描、连接与设备名
#include "esp_bt_main.h"      // Bluedroid 协议栈引擎初始化与使能

static const char *TAG = "EXP1_BLE_ADV";

/* 广播给手机看到的蓝牙设备名称 */
#define DEVICE_NAME "ESP32-Journey-Beacon"

/**
 * 📌 结构体 1：BLE GAP 广播底层物理参数 (adv_params)
 * 比喻：这相当于给无线电大喇叭配置“喊话频率”与“喊话策略”。
 */
static esp_ble_adv_params_t adv_params = {
    // 广播时间间隔计算公式：数值 * 0.625ms
    // 0x20 = 32 * 0.625ms = 20ms（最小间隔）
    // 0x40 = 64 * 0.625ms = 40ms（最大间隔）
    // 含义：ESP32 芯片每隔 20ms ~ 40ms 瞬间醒来 1 毫秒发射一次广播脉冲，其余时间极低功耗深度休眠！
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,

    // 广播类型：ADV_TYPE_IND 代表“通用可连接广播”（手机既能扫描到它，也能发起连接）
    .adv_type           = ADV_TYPE_IND,

    // 自身蓝牙 MAC 地址类型：使用出厂烧录的公开物理 MAC 地址
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,

    // 广播信道映射：ADV_CHNL_ALL 代表在 37、38、39 三个广播信道轮流发射，抗干扰能力最强
    .channel_map        = ADV_CHNL_ALL,

    // 过滤策略：允许任何手机扫描、允许任何手机发起连接
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/**
 * 📌 结构体 2：BLE 广播数据包内容 (adv_data)
 * 比喻：无线电广播里到底包含什么具体内容？（如名字、发射功率等）
 * 注意：BLE 广播包 Payload 最大长度为 31 字节，内容要精炼！
 */
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false, // false 表示这是普通广播包，而不是扫描响应包（Scan Response）
    .include_name        = true,  // 把设备名称包含在广播包中，手机一搜就能看到中文/英文名字
    .include_txpower     = true,  // 包含发射功率强度（手机可据此粗略估算与 ESP32 的物理距离）
    .min_interval        = 0x0006,// 从机期望的连接间隔下限 (6 * 1.25ms = 7.5ms)
    .max_interval        = 0x0010,// 从机期望的连接间隔上限 (16 * 1.25ms = 20ms)
    .appearance          = 0x00,  // 设备外观图标类别代码（0x00 为通用未分类设备）
    .manufacturer_len    = 0,     // 自定义厂商数据长度（此处暂不携带）
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = 0,
    .p_service_uuid      = NULL,
    // 广播 Flags 标志位：
    // ESP_BLE_ADV_FLAG_GEN_DISC: 支持普通可发现模式（设备会一直广播，手机随时能搜到）
    // ESP_BLE_ADV_FLAG_BREDR_NOT_SPT: 不支持传统经典蓝牙（纯 BLE 低功耗模式）
    .flag                = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

/**
 * 📌 GAP 事件回调函数（GAP Event Handler）
 * 蓝牙底层是全异步事件驱动的。当我们调用配置广播接口后，底层配置完成会通过该回调异步通知应用层。
 */
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        // 事件 1：广播数据配置成功事件（底层已将 adv_data 写入基带芯片寄存器）
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            ESP_LOGI(TAG, "📡 广播数据已配置写入基带，正在开启 BLE 射频广播...");
            // 数据准备就绪，正式启动射频发射！
            esp_ble_gap_start_advertising(&adv_params);
            break;

        // 事件 2：广播启动结果事件（反馈广播发射是否成功）
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "🎉 [BLE 广播成功启动] 射频天线正在持续发射！");
                ESP_LOGI(TAG, "👉 请打开手机 LightBlue App 搜索: \033[32m%s\033[0m", DEVICE_NAME);
            } else {
                ESP_LOGE(TAG, "❌ BLE 广播启动失败, 错误码: %d", param->adv_start_cmpl.status);
            }
            break;

        default:
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 14 实验 1：BLE 广播与手机扫描发现       ");
    ESP_LOGI(TAG, "==================================================");

    /* 步骤 1：初始化 NVS Flash（蓝牙协议栈底层需要使用 NVS 保存校准参数与配对密钥） */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 步骤 2：释放经典蓝牙内存（经典蓝牙占用约 30KB 内存，我们只用 BLE，释放可极大地节省 RAM） */
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    /* 步骤 3：初始化底层硬件 BT 控制器并使能 BLE 单模工作方式 */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    /* 步骤 4：初始化并启动 Bluedroid 蓝牙协议栈主机引擎 */
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    /* 步骤 5：配置 GAP 层并启动广播 */
    // 5.1 注册 GAP 事件回调函数
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    // 5.2 设置手机扫描时看到的蓝牙设备广播名
    ESP_ERROR_CHECK(esp_ble_gap_set_device_name(DEVICE_NAME));
    // 5.3 提交广播数据包配置（触发 ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT 事件，进而在回调中开启广播）
    ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&adv_data));

    /* 主任务进入低功耗待机循环，蓝牙广播全由底层协议栈和基带硬件在后台自动运行 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
