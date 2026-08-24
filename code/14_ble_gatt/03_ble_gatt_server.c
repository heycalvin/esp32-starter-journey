/**
 * 🌟 ESP32 物联网实战 —— 第 14 关 实验 3：GATT Server 特征值读写与数据透传 (Read/Write)
 * 
 * 🎯 学习目标与背景铺垫：
 *    1. 【GATT 树状结构】：Profile ➔ Service (0x00FF) ➔ Characteristic (0xFF01)。
 *    2. 【GATTS 状态机流水线】：
 *       ① 注册 Application ID ➔ 触发 ESP_GATTS_REG_EVT（在此事件中创建 Service）；
 *       ② Service 创建成功 ➔ 触发 ESP_GATTS_CREATE_EVT（在此启动 Service 并添加 Characteristic）；
 *       ③ 特征值添加成功 ➔ 触发 ESP_GATTS_ADD_CHAR_EVT（在此开始广播，等待手机连接）；
 *       ④ 手机连接成功 ➔ 触发 ESP_GATTS_CONNECT_EVT；
 *       ⑤ 手机写入数据 ➔ 触发 ESP_GATTS_WRITE_EVT（解析数据控制 LED2，并回复 Write Response）；
 *       ⑥ 手机读取数据 ➔ 触发 ESP_GATTS_READ_EVT（回复字符串 "ESP32_OK"）。
 * 
 * 📌 硬件接口：
 *    - 板载绿色 LED2: GPIO27 (高电平点亮，低电平熄灭)
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

static const char *TAG = "EXP3_GATT_SERVER";

#define LED2_PIN               GPIO_NUM_27
#define DEVICE_NAME            "ESP32-GATT-Server"

/* 自定义 GATT 服务与特征值 16位短 UUID */
#define GATTS_SERVICE_UUID_TEST     0x00FF  // 自定义服务 UUID: 0x00FF (专柜门牌)
#define GATTS_CHAR_UUID_TEST        0xFF01  // 自定义特征 UUID: 0xFF01 (抽屉门牌)
#define GATTS_NUM_HANDLE_TEST       4       // 该服务预留的 Handle 数量（服务声明+特征声明+特征值+描述符）

/* 运行时句柄缓存 */
static uint16_t s_service_handle = 0;
static uint16_t s_char_handle = 0;
static uint16_t s_conn_id = 0;
static bool s_is_connected = false;

/* 广播物理参数：20ms~40ms 广播一次 */
static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* 广播内容包：包含设备名和发射功率 */
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = true,
    .flag                = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

/* GAP 事件回调：负责处理底层广播开启与状态 */
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            ESP_LOGI(TAG, "📡 [BLE 广播中] 手机 App 可连接: \033[36m%s\033[0m", DEVICE_NAME);
            break;
        default:
            break;
    }
}

/**
 * 📌 GATTS 事件回调函数：处理所有 GATT 服务层核心业务
 */
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        /* 事件 1：应用注册成功 ➔ 创建服务 */
        case ESP_GATTS_REG_EVT: {
            ESP_LOGI(TAG, "🚀 [Step 1] GATT 接口注册成功，正在创建 Service (UUID: 0x%04X)...", GATTS_SERVICE_UUID_TEST);
            esp_gatt_srvc_id_t service_id = {
                .is_primary = true, // 主服务
                .id = {
                    .inst_id = 0x00,
                    .uuid = {
                        .len = ESP_UUID_LEN_16,
                        .uuid = { .uuid16 = GATTS_SERVICE_UUID_TEST },
                    },
                },
            };
            esp_ble_gatts_create_service(gatts_if, &service_id, GATTS_NUM_HANDLE_TEST);
            break;
        }

        /* 事件 2：服务创建成功 ➔ 启动服务并向其添加特征值 Characteristic */
        case ESP_GATTS_CREATE_EVT: {
            s_service_handle = param->create.service_handle;
            ESP_LOGI(TAG, "🏢 [Step 2] Service 创建成功 (Handle: 0x%04x)，正在启动并添加 Characteristic (UUID: 0x%04X)...",
                     s_service_handle, GATTS_CHAR_UUID_TEST);
            
            esp_ble_gatts_start_service(s_service_handle);

            esp_bt_uuid_t char_uuid = {
                .len = ESP_UUID_LEN_16,
                .uuid = { .uuid16 = GATTS_CHAR_UUID_TEST },
            };
            esp_ble_gatts_add_char(s_service_handle, &char_uuid,
                                   ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                   ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
                                   NULL, NULL);
            break;
        }

        /* 事件 3：特征值添加成功 ➔ 此时整个 GATT 树搭建完毕，可以对外配置广播数据了 */
        case ESP_GATTS_ADD_CHAR_EVT:
            s_char_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "✅ [Step 3] Characteristic 添加就绪 (Char Handle: 0x%04x)，开始配置广播！", s_char_handle);
            esp_ble_gap_config_adv_data(&adv_data);
            break;

        /* 事件 4：手机成功连接到 ESP32 */
        case ESP_GATTS_CONNECT_EVT: {
            s_conn_id = param->connect.conn_id;
            s_is_connected = true;
            ESP_LOGI(TAG, "🎉 [Step 4: 手机已连接] Connection ID: %d, MAC 地址已握手！", s_conn_id);

            /* ⭐️ 核心防断连机制：主动向手机申请优化连接参数 */
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            conn_params.min_int = 0x10; // 最小连接间隔: 20ms
            conn_params.max_int = 0x20; // 最大连接间隔: 40ms
            conn_params.latency = 0;    // 从机延迟: 0
            conn_params.timeout = 400;  // 监督超时: 4000ms (4秒)
            esp_ble_gap_update_conn_params(&conn_params);
            break;
        }

        /* 事件 5：手机断开连接 ➔ 重新开启广播，等待下一次连接 */
        case ESP_GATTS_DISCONNECT_EVT:
            s_is_connected = false;
            ESP_LOGW(TAG, "⚠️ 手机已断开连接，正在重新开启 BLE 广播...");
            esp_ble_gap_start_advertising(&adv_params);
            break;

        /* 事件 6：手机主动读取特征值 (Read) */
        case ESP_GATTS_READ_EVT: {
            ESP_LOGI(TAG, "📖 [手机发起读取 Read] 收到请求, Handle: 0x%04x", param->read.handle);
            esp_gatt_rsp_t rsp;
            memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
            rsp.attr_value.handle = param->read.handle;
            const char *reply_str = "ESP32_OK";
            rsp.attr_value.len = strlen(reply_str);
            memcpy(rsp.attr_value.value, reply_str, rsp.attr_value.len);
            esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
            ESP_LOGI(TAG, "📤 已向手机回复数据: %s", reply_str);
            break;
        }

        /* 事件 7：⭐️ 手机主动写入特征值 (Write) ➔ 控制 LED2 */
        case ESP_GATTS_WRITE_EVT: {
            ESP_LOGI(TAG, "✍️ [手机写入数据 Write] 长度: %d 字节, 原始内容: \033[32m%.*s\033[0m",
                     param->write.len, param->write.len, param->write.value);

            if (param->write.len > 0) {
                char cmd = param->write.value[0];
                if (cmd == '1') {
                    gpio_set_level(LED2_PIN, 1);
                    ESP_LOGI(TAG, "💡 收到 '1' ➔ 点亮板载绿色 LED2 (GPIO27)");
                } else if (cmd == '0') {
                    gpio_set_level(LED2_PIN, 0);
                    ESP_LOGI(TAG, "🌑 收到 '0' ➔ 熄灭板载绿色 LED2 (GPIO27)");
                }
            }

            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            break;
        }

        default:
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 14 实验 3：GATT 特征值读写与数据透传     ");
    ESP_LOGI(TAG, "==================================================");

    /* 1. 初始化 GPIO27 为输出引脚（控制板载 LED2） */
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED2_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_conf);
    gpio_set_level(LED2_PIN, 0);

    /* 2. 初始化 NVS Flash */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 3. 释放经典蓝牙内存，节省约 30KB RAM */
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

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
