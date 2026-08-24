/**
 * 🌟 ESP32 物联网实战 —— 第 15 关 实验 1：ESP-NOW 双机单播对射遥控 (P2P Unicast)
 * 
 * 🎯 学习目标：
 *    1. 掌握乐鑫独家 ESP-NOW 协议的底层工作原理与极速免握手特性；
 *    2. 掌握 Wi-Fi STA 模式初始化与 MAC 地址点名配对机制（Peer 对端管理）；
 *    3. 实现板 A 按下 SW3 按键，板 B 毫秒级（< 2ms）翻转板载绿色 LED2；
 *    4. 理解底层硬件 ACK 回执状态监听（esp_now_register_send_cb）。
 * 
 * 📌 硬件引脚分配：
 *    - 板载 LED2：     GPIO27 (高电平点亮)
 *    - 用户按键 SW3：  GPIO39 (输入专用，低电平有效)
 * 
 * 💡 小白单板/双板调试指南：
 *    - 无论你有 1 块还是 2 块板子，开机都会在串口打印当前板子的【真实 MAC 地址】；
 *    - 若有 2 块板子：将板 A 打印出的 MAC 填入板 B 的 PEER_MAC 宏中，反之亦然；
 *    - 若只有 1 块板子：保持默认广播/回环测试，同样能完整观察 ESP-NOW 的发包回调与协议流程！
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"

static const char *TAG = "EXP1_ESPNOW_UNICAST";

#define LED2_PIN        GPIO_NUM_27
#define BUTTON_PIN      GPIO_NUM_39

/* ==============================================================================
 * 🎯 目标接收端 MAC 地址配置 (6 字节)
 * 
 * 💡 提示：将另一块 ESP32 开机串口输出的 MAC 地址填入此处即可建立点对点单播信道！
 * 默认填入全 FF 即为通用广播地址 (所有周围的 ESP32 都能收到)
 * ============================================================================== */
static uint8_t s_peer_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/* 自定义单播遥控数据包结构体 (最大 250 字节) */
typedef struct {
    uint32_t seq_num;       // 发送序列号 (第几次按下按键)
    uint8_t  cmd;           // 控制指令: 1 ➔ 点亮 LED, 0 ➔ 熄灭 LED, 2 ➔ 翻转 LED
    char     sender_name[16]; // 发送者昵称
} __attribute__((packed)) remote_packet_t;

static bool s_led_state = false;
static uint8_t s_my_mac[6] = {0};

/**
 * 📡 ESP-NOW 数据发送完成回调函数 (底层硬件 ACK 回执)
 * 
 * 当调用 esp_now_send 发出数据后，Wi-Fi 射频硬件会自动等待对方的 802.11 ACK 帧，
 * 收到 ACK 则 status 为 ESP_NOW_SEND_SUCCESS，否则为 ESP_NOW_SEND_FAIL。
 */
static void on_data_sent(const esp_now_send_info_t *info, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "🚀 [硬件 ACK 成功] 数据包已成功投递到目标设备！(耗时 < 2ms)");
    } else {
        ESP_LOGW(TAG, "⚠️ [发送无响应] 未收到对端 ACK 确认 (目标设备可能关机或距离过远)");
    }
}

/**
 * 📥 ESP-NOW 数据接收回调函数 (收到数据瞬间触发)
 */
static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (len == sizeof(remote_packet_t)) {
        remote_packet_t *pkt = (remote_packet_t *)data;
        ESP_LOGI(TAG, "📥 [收到对端遥控指令] 来源 MAC: %02X:%02X:%02X:%02X:%02X:%02X | 发送者: %s | 序号: #%lu | 指令: %d",
                 recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
                 recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5],
                 pkt->sender_name, pkt->seq_num, pkt->cmd);

        // 执行动作：翻转 LED2
        if (pkt->cmd == 2 || pkt->cmd == 1) {
            s_led_state = !s_led_state;
            gpio_set_level(LED2_PIN, s_led_state ? 1 : 0);
            ESP_LOGI(TAG, "💡 执行受控动作 ➔ 板载绿色 LED2 状态已翻转为: \033[32m%s\033[0m",
                     s_led_state ? "【点亮 ON】" : "【熄灭 OFF】");
        }
    } else {
        ESP_LOGW(TAG, "⚠️ 收到未知格式数据包，长度: %d 字节", len);
    }
}

/**
 * 📌 Wi-Fi 底层初始化与 ESP-NOW 协议栈挂载
 */
static void wifi_espnow_init(void)
{
    // 1. 初始化 TCP/IP 与默认事件循环 (ESP-NOW 依赖 Wi-Fi 底层驱动)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 2. 初始化 Wi-Fi 驱动为 STA 模式 (无需连接任何路由器！)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 3. 读取并打印本机物理 MAC 地址
    esp_read_mac(s_my_mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "🏷️  本机 Wi-Fi STA MAC 地址: \033[36m%02X:%02X:%02X:%02X:%02X:%02X\033[0m",
             s_my_mac[0], s_my_mac[1], s_my_mac[2], s_my_mac[3], s_my_mac[4], s_my_mac[5]);
    ESP_LOGI(TAG, "==================================================");

    // 4. 初始化 ESP-NOW 协议栈
    ESP_ERROR_CHECK(esp_now_init());

    // 5. 注册发送与接收事件回调
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));

    // 6. 将目标设备添加为 Peer (对端通信伙伴)
    esp_now_peer_info_t peer_info = {
        .channel = 0,         // 0 代表跟随当前 Wi-Fi 信道
        .ifidx = WIFI_IF_STA, // 走 STA 接口射频
        .encrypt = false,     // 基础实验不加密，追求极致极限速度
    };
    memcpy(peer_info.peer_addr, s_peer_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));

    ESP_LOGI(TAG, "✅ ESP-NOW 协议栈就绪！已配对 Peer: %02X:%02X:%02X:%02X:%02X:%02X",
             s_peer_mac[0], s_peer_mac[1], s_peer_mac[2], s_peer_mac[3], s_peer_mac[4], s_peer_mac[5]);
}

/**
 * 🔘 独立 FreeRTOS 任务：后台监听板载 SW3 按键，按下时通过 ESP-NOW 发射遥控帧
 */
static void button_remote_task(void *pvParameters)
{
    uint32_t click_count = 0;
    while (1) {
        // SW3 (GPIO39) 默认高电平，按下为低电平 0
        if (gpio_get_level(BUTTON_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(20)); // 消抖 20ms
            if (gpio_get_level(BUTTON_PIN) == 0) {
                click_count++;
                ESP_LOGI(TAG, "🔘 [SW3 按键按下] 触发第 %lu 次极速对射遥控发包...", click_count);

                // 组装对射数据帧
                remote_packet_t pkt = {
                    .seq_num = click_count,
                    .cmd = 2, // 翻转指令
                };
                snprintf(pkt.sender_name, sizeof(pkt.sender_name), "ESP32_%02X%02X", s_my_mac[4], s_my_mac[5]);

                // 🚀 调用底层 API 极速对射发送！
                esp_err_t err = esp_now_send(s_peer_mac, (uint8_t *)&pkt, sizeof(pkt));
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "❌ esp_now_send 失败，错误码: %d", err);
                }

                // 等待按键抬起
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
    ESP_LOGI(TAG, "   🚀 关卡 15 实验 1：ESP-NOW 双机单播对射遥控     ");
    ESP_LOGI(TAG, "==================================================");

    // 1. 初始化板载 LED2 (GPIO27)
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED2_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_conf);
    gpio_set_level(LED2_PIN, 0);

    // 2. 初始化板载按键 SW3 (GPIO39)
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
    };
    gpio_config(&btn_conf);

    // 3. 初始化 NVS 闪存 (Wi-Fi 底层存储依赖)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 4. 启动 Wi-Fi 与 ESP-NOW 协议栈
    wifi_espnow_init();

    // 5. 启动按键遥控发射任务
    xTaskCreate(button_remote_task, "btn_remote_task", 3072, NULL, 5, NULL);
}
