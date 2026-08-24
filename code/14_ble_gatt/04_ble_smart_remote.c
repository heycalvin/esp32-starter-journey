/**
 * 🌟 ESP32 物联网实战 —— 第 14 关 实验 4：手机 BLE 遥控器与主动 Notify 状态推送 (综合大工程)
 * 
 * 🎯 学习目标与背景铺垫：
 *    1. 【双向通信闭环】：
 *       - 下行控制（手机 ➔ ESP32 Write）：手机发送 '1'/'0' 开关绿色 LED2；
 *       - 上行推送（ESP32 ➔ 手机 Notify）：当检测到板载按键 SW3 被按下，单片机主动将最新数据推给手机。
 *    2. 【为什么 Notify 最优？】：
 *       - 传统轮询（手机每隔 100ms 读一次）：浪费 99% 的无线带宽与手机电量；
 *       - 硬件主动推送（Notify）：平时完全静默，一旦按键触发，单片机立即弹射数据包，毫秒级响应！
 *    3. 【系统协同架构】：
 *       - 主任务：初始化 GPIO、NVS、BT 控制器与 Bluedroid 协议栈；
 *       - FreeRTOS 独立按键任务：以 50ms 周期防抖扫描 SW3，一旦触发，调用 `esp_ble_gatts_send_indicate` 推送。
 * 
 * 📌 硬件接口：
 *    - 板载绿色 LED2: GPIO27 (高电平点亮，低电平熄灭)
 *    - 用户按键 SW3: GPIO39 (VN 输入专用管脚，按下时为低电平 0)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

/* 蓝牙核心头文件 */
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

static const char *TAG = "EXP4_BLE_REMOTE";

#define LED2_PIN               GPIO_NUM_27
#define BUTTON_PIN             GPIO_NUM_39
#define DEVICE_NAME            "ESP32-Smart-Remote"

/* ==============================================================================
 * 🎯 主动推送模式选择开关 (0 或 1)
 * 
 * 模式 0【Notify 免确认极速通知】：
 *    - 机制：need_confirm = false，单片机发完即走，延迟 < 2ms，高吞吐；
 *    - 场景：手环计步/心率、按键连击、遥控器摇杆坐标。
 * 
 * 模式 1【Indicate 带签收回执指示】：
 *    - 机制：need_confirm = true，单片机发送后等待手机回复 ACK (触发 ESP_GATTS_CONF_EVT)；
 *    - 场景：门锁开门确认回执、火灾报警鸣响、OTA 固件包分片。
 * ============================================================================== */
#define PUSH_MODE_INDICATE     0   // 👉 改为 1 即可体验 Indicate 顺丰签字签收模式！

/* 自定义 GATT 服务与特征值 UUID */
#define GATTS_SERVICE_UUID_HUB      0xFFE0  // 智能遥控服务 UUID: 0xFFE0
#define GATTS_CHAR_UUID_HUB         0xFFE1  // 双向透传特征值 UUID: 0xFFE1 (同时支持 Read/Write/Notify/Indicate)
#define GATTS_NUM_HANDLE_HUB        4

/* 运行时全局句柄 */
static uint16_t s_service_handle = 0;
static uint16_t s_char_handle = 0;
static esp_gatt_if_t s_gatts_if = 0;
static uint16_t s_conn_id = 0;
static bool s_is_connected = false;
static bool s_led_state = false;

/* 广播物理参数 */
static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* 广播内容包 */
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = true,
    .flag                = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            ESP_LOGI(TAG, "📡 [BLE 遥控器广播中] 手机 App 请搜索连接: \033[36m%s\033[0m", DEVICE_NAME);
            break;
        default:
            break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT: {
            s_gatts_if = gatts_if; // 保存 GATT 接口句柄供推送使用
            esp_gatt_srvc_id_t service_id = {
                .is_primary = true,
                .id = {
                    .inst_id = 0x00,
                    .uuid = {
                        .len = ESP_UUID_LEN_16,
                        .uuid = { .uuid16 = GATTS_SERVICE_UUID_HUB },
                    },
                },
            };
            esp_ble_gatts_create_service(gatts_if, &service_id, GATTS_NUM_HANDLE_HUB);
            break;
        }
        case ESP_GATTS_CREATE_EVT: {
            s_service_handle = param->create.service_handle;
            esp_ble_gatts_start_service(s_service_handle);

            // ⭐️ 添加全功能特征值：同时开启 READ、WRITE、NOTIFY 和 INDICATE 四大属性权限
            esp_bt_uuid_t char_uuid = {
                .len = ESP_UUID_LEN_16,
                .uuid = { .uuid16 = GATTS_CHAR_UUID_HUB },
            };
            esp_ble_gatts_add_char(s_service_handle, &char_uuid,
                                   ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                   ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE |
                                   ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_INDICATE,
                                   NULL, NULL);
            break;
        }
        case ESP_GATTS_ADD_CHAR_EVT:
            s_char_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "✅ BLE 遥控服务与特征值就绪 (Char Handle: 0x%04x)", s_char_handle);
            esp_ble_gap_config_adv_data(&adv_data);
            break;

        case ESP_GATTS_CONNECT_EVT: {
            s_conn_id = param->connect.conn_id;
            s_is_connected = true;
            ESP_LOGI(TAG, "🎉 [手机已连接 BLE 遥控器] Conn ID: %d, 当前推送模式: \033[33m%s\033[0m",
                     s_conn_id, (PUSH_MODE_INDICATE == 0) ? "Notify 免确认模式" : "Indicate 签收确认模式");

            /* ⭐️ 核心防断连机制：主动向手机申请优化连接参数 (保活心跳) */
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            conn_params.min_int = 0x10; // 最小连接间隔: 16 * 1.25ms = 20ms
            conn_params.max_int = 0x20; // 最大连接间隔: 32 * 1.25ms = 40ms
            conn_params.latency = 0;    // 从机延迟: 0
            conn_params.timeout = 400;  // 监督超时: 400 * 10ms = 4000ms (4秒)
            esp_ble_gap_update_conn_params(&conn_params);
            break;
        }

        case ESP_GATTS_DISCONNECT_EVT:
            s_is_connected = false;
            ESP_LOGW(TAG, "⚠️ 手机已断开，重新启动 BLE 广播...");
            esp_ble_gap_start_advertising(&adv_params);
            break;

        /* 手机下发控制数据 (Write) */
        case ESP_GATTS_WRITE_EVT: {
            ESP_LOGI(TAG, "📥 [收到手机下发控制指令] \033[32m%.*s\033[0m",
                     param->write.len, param->write.value);

            if (param->write.len > 0) {
                if (param->write.value[0] == '1') {
                    s_led_state = true;
                    gpio_set_level(LED2_PIN, 1);
                    ESP_LOGI(TAG, "💡 手机指令 ➔ 点亮板载绿色 LED2");
                } else if (param->write.value[0] == '0') {
                    s_led_state = false;
                    gpio_set_level(LED2_PIN, 0);
                    ESP_LOGI(TAG, "🌑 手机指令 ➔ 熄灭板载绿色 LED2");
                }
            }

            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            break;
        }

        /* ⭐️ 核心：Indicate 模式下收到手机端的签收确认回执 (ACK) */
        case ESP_GATTS_CONF_EVT:
            if (param->conf.status == ESP_GATT_OK) {
                ESP_LOGI(TAG, "✍️ [Indicate 签收成功] 手机端已回执确认收到数据包！(Status: OK)");
            } else {
                ESP_LOGW(TAG, "⚠️ [Indicate 签收异常] 错误码: %d", param->conf.status);
            }
            break;

        default:
            break;
    }
}

/**
 * 📌 独立 FreeRTOS 任务：后台监听板载 SW3 按键，按下时主动弹射推送给手机
 */
static void button_notify_task(void *pvParameters)
{
    int click_count = 0;
    while (1) {
        // SW3 (GPIO39) 默认高电平，按下为低电平 0
        if (gpio_get_level(BUTTON_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(20)); // 软件消抖 20ms
            if (gpio_get_level(BUTTON_PIN) == 0) {
                click_count++;
                ESP_LOGI(TAG, "🔘 检测到 SW3 按键按下 (累计第 %d 次)", click_count);

                // 只有在手机已连接且特征值有效时才推送
                if (s_is_connected && s_char_handle != 0) {
                    char notify_msg[64];
                    snprintf(notify_msg, sizeof(notify_msg), "SW3_CLICK_%d", click_count);

#if PUSH_MODE_INDICATE == 0
                    // ⭐️ 模式 0：以 Notify（免确认）极速模式发送 (need_confirm = false)
                    esp_ble_gatts_send_indicate(s_gatts_if, s_conn_id, s_char_handle,
                                               strlen(notify_msg), (uint8_t *)notify_msg, false);
                    ESP_LOGI(TAG, "📤 [主动 Notify 弹射推送给手机] ➔ \"%s\"", notify_msg);
#else
                    // ⭐️ 模式 1：以 Indicate（需手机确认 ACK）模式发送 (need_confirm = true)
                    esp_ble_gatts_send_indicate(s_gatts_if, s_conn_id, s_char_handle,
                                               strlen(notify_msg), (uint8_t *)notify_msg, true);
                    ESP_LOGI(TAG, "📤 [主动 Indicate 带回执推送] ➔ \"%s\" (等待手机 ACK...)", notify_msg);
#endif
                } else {
                    ESP_LOGW(TAG, "⚠️ 手机未连接，暂无法推送消息");
                }

                // 等待按键抬起（防止长按重复触发）
                while (gpio_get_level(BUTTON_PIN) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 14 实验 4：BLE 智能遥控与主动 Notify 推送 ");
    ESP_LOGI(TAG, "==================================================");

    /* 1. 初始化板载 LED2 (GPIO27 输出) 与 用户按键 SW3 (GPIO39 输入) */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED2_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    gpio_set_level(LED2_PIN, 0);

    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
    };
    gpio_config(&btn_conf);

    /* 2. 初始化 NVS Flash */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 3. 释放经典蓝牙内存，节省 RAM */
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    /* 4. 初始化硬件 BT 控制器并使能 BLE */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    /* 5. 初始化并启动 Bluedroid 协议栈 */
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    /* 6. 注册 GATTS 与 GAP 事件回调，并启动 GATT 应用程序注册 */
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_set_device_name(DEVICE_NAME));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(0));

    /* 7. 创建独立的 FreeRTOS 按键监听与 Notify 推送任务 */
    xTaskCreate(button_notify_task, "btn_notify", 3072, NULL, 5, NULL);
}
