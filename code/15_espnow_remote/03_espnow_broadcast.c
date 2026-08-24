/**
 * 🌟 ESP32 物联网实战 —— 第 15 关 实验 3：ESP-NOW 广播群控与“一呼百应” (Broadcast Group Control)
 * 
 * 🎯 学习目标：
 *    1. 掌握 ESP-NOW 广播信道机制（目标 MAC: FF:FF:FF:FF:FF:FF）；
 *    2. 实现发射端一键按下 SW3 按键，周围所有 ESP32 从机 2ms 内“一呼百应”同步动作；
 *    3. 设计多模式群控协议（全开、全关、全闪烁、全彩协同）；
 *    4. 理解无人机灯光秀、智能农业灌溉阀门阵列等工业群控背后的架构精髓。
 * 
 * 📌 硬件引脚分配：
 *    - 板载 LED2：     GPIO27 (高电平点亮)
 *    - 用户按键 SW3：  GPIO39 (输入专用，低电平有效，按下切换群控模式)
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

static const char *TAG = "EXP3_ESPNOW_BROADCAST";

#define LED2_PIN        GPIO_NUM_27
#define BUTTON_PIN      GPIO_NUM_39

/* 全局广播 MAC 地址 (一呼百应) */
static const uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/* 群控指令模式枚举 */
typedef enum {
    GROUP_CMD_ALL_OFF = 0,    // 全灭
    GROUP_CMD_ALL_ON  = 1,    // 全亮
    GROUP_CMD_SYNC_BLINK = 2, // 同步闪烁 3 次
} group_cmd_t;

/* 广播群控报文协议 */
typedef struct {
    uint8_t  magic;           // 协议魔数: 0x5A (防误触发)
    uint8_t  cmd;             // 群控指令 (group_cmd_t)
    uint32_t group_seq;       // 群控全局序号
    char     commander[16];   // 指挥官名称
} __attribute__((packed)) group_control_packet_t;

static uint8_t s_my_mac[6] = {0};
static uint8_t s_current_mode = 0;

static void on_broadcast_sent(const esp_now_send_info_t *info, esp_now_send_status_t status)
{
    // 注意：广播报文在物理层不要求单个设备回复 ACK，status 为 SUCCESS 代表射频硬件成功发射上空
    ESP_LOGI(TAG, "📢 [广播射频发射完成] 802.11 广播帧已送达周围空域！");
}

static void on_broadcast_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (len != sizeof(group_control_packet_t)) return;

    group_control_packet_t *pkt = (group_control_packet_t *)data;

    // 校验魔数
    if (pkt->magic != 0x5A) return;

    ESP_LOGI(TAG, "⚡ [收到集群广播指令] 来源指挥官: \033[36m%s\033[0m | 序号: #%lu | 指令代码: %d",
             pkt->commander, pkt->group_seq, pkt->cmd);

    // 执行群控指令
    switch (pkt->cmd) {
        case GROUP_CMD_ALL_OFF:
            gpio_set_level(LED2_PIN, 0);
            ESP_LOGI(TAG, "🌑 [群控执行] 全体节点协同 ➔ \033[31m【全部熄灭】\033[0m");
            break;

        case GROUP_CMD_ALL_ON:
            gpio_set_level(LED2_PIN, 1);
            ESP_LOGI(TAG, "💡 [群控执行] 全体节点协同 ➔ \033[32m【全部点亮】\033[0m");
            break;

        case GROUP_CMD_SYNC_BLINK:
            ESP_LOGI(TAG, "✨ [群控执行] 全体节点协同 ➔ \033[33m【同步律动闪烁 3 次】\033[0m");
            for (int i = 0; i < 3; i++) {
                gpio_set_level(LED2_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(LED2_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            break;

        default:
            break;
    }
}

static void wifi_espnow_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_read_mac(s_my_mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "🏷️  本机节点 MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             s_my_mac[0], s_my_mac[1], s_my_mac[2], s_my_mac[3], s_my_mac[4], s_my_mac[5]);

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_broadcast_sent));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_broadcast_recv));

    // 添加广播地址 Peer
    esp_now_peer_info_t peer_info = {
        .channel = 0,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer_info.peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));

    ESP_LOGI(TAG, "✅ 广播群控信道已建立 (Peer: FF:FF:FF:FF:FF:FF)");
}

/**
 * 🔘 按键群控任务：每按一次 SW3 循环切换群控指令模式并一呼百应广播
 */
static void commander_button_task(void *pvParameters)
{
    uint32_t broadcast_seq = 0;
    while (1) {
        if (gpio_get_level(BUTTON_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(20)); // 消抖
            if (gpio_get_level(BUTTON_PIN) == 0) {
                broadcast_seq++;
                s_current_mode = (s_current_mode + 1) % 3; // 0 ➔ 1 ➔ 2 循环

                group_control_packet_t pkt = {
                    .magic = 0x5A,
                    .cmd = s_current_mode,
                    .group_seq = broadcast_seq,
                };
                snprintf(pkt.commander, sizeof(pkt.commander), "CMD_%02X%02X", s_my_mac[4], s_my_mac[5]);

                ESP_LOGI(TAG, "📢 [指挥官发令] 按下 SW3 ➔ 发射全局广播群控令 (模式: %d, 序号: #%lu)...",
                         s_current_mode, broadcast_seq);

                esp_now_send(s_broadcast_mac, (uint8_t *)&pkt, sizeof(pkt));

                // 自身也执行一次（保持群控一致性）
                if (s_current_mode == 0) gpio_set_level(LED2_PIN, 0);
                else if (s_current_mode == 1) gpio_set_level(LED2_PIN, 1);
                else if (s_current_mode == 2) {
                    for (int i = 0; i < 3; i++) {
                        gpio_set_level(LED2_PIN, 1);
                        vTaskDelay(pdMS_TO_TICKS(100));
                        gpio_set_level(LED2_PIN, 0);
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                }

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
    ESP_LOGI(TAG, "   🚀 关卡 15 实验 3：ESP-NOW 广播群控“一呼百应”  ");
    ESP_LOGI(TAG, "==================================================");

    gpio_config_t led_conf = { .pin_bit_mask = (1ULL << LED2_PIN), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&led_conf);
    gpio_set_level(LED2_PIN, 0);

    gpio_config_t btn_conf = { .pin_bit_mask = (1ULL << BUTTON_PIN), .mode = GPIO_MODE_INPUT };
    gpio_config(&btn_conf);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_espnow_init();

    xTaskCreate(commander_button_task, "commander_btn_task", 3072, NULL, 5, NULL);
}
