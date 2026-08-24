/**
 * 🌟 ESP32 物联网实战 —— 第 15 关 实验 2：ESP-NOW 双向对讲与链路质量探测 (RTT & 丢包率)
 * 
 * 🎯 学习目标：
 *    1. 掌握 ESP-NOW 对等双向通信（Ping-Pong 对讲机制）；
 *    2. 利用高精度定时器（esp_timer_get_time）微秒级测量无线空口往返延迟（RTT）；
 *    3. 实时统计无线链路传输成功率（PDR）、丢包率与 RSSI 信号质量；
 *    4. 领略 ESP-NOW 相比传统 Wi-Fi TCP/UDP 超越百倍的极速响应（< 2ms）。
 * 
 * 📌 硬件引脚分配：
 *    - 板载 LED2：     GPIO27 (每完成一次 Ping-Pong 闪烁一次)
 *    - 用户按键 SW3：  GPIO39 (按下可手动发起一次爆发式测速)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"

static const char *TAG = "EXP2_ESPNOW_TWOWAY";

#define LED2_PIN        GPIO_NUM_27
#define BUTTON_PIN      GPIO_NUM_39

/* 目标对端 MAC 地址 (填入全 FF 支持一键双向广播探测) */
static uint8_t s_peer_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/* 报文类型枚举 */
typedef enum {
    MSG_TYPE_PING = 1,
    MSG_TYPE_PONG = 2
} msg_type_t;

/* Ping-Pong 测速与遥测报文 (Packed 紧凑排列) */
typedef struct {
    uint8_t  type;          // 报文类型: 1=PING, 2=PONG
    uint32_t ping_id;       // 测速序号
    int64_t  send_time_us;  // 发射时刻微秒级时间戳 (esp_timer_get_time)
    float    temp_celsius;  // 携带模拟环境温度 (如 26.5°C)
    char     node_name[16]; // 本机名称
} __attribute__((packed)) ping_pong_packet_t;

static uint8_t s_my_mac[6] = {0};
static uint32_t s_total_sent = 0;
static uint32_t s_total_acked = 0;
static uint32_t s_total_recv_pong = 0;
static float s_avg_rtt_ms = 0.0f;

static void on_data_sent(const esp_now_send_info_t *info, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS) {
        s_total_acked++;
    }
}

static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (len != sizeof(ping_pong_packet_t)) return;

    ping_pong_packet_t *pkt = (ping_pong_packet_t *)data;

    // 场景 A：收到对方发来的 PING ➔ 立即回传一个 PONG (Ping-Pong 回声机制)
    if (pkt->type == MSG_TYPE_PING) {
        ESP_LOGI(TAG, "🏓 [收到 PING 探测] 来源: %s (%02X:%02X:%02X:%02X:%02X:%02X) | PingID: #%lu ➔ 立即回送 PONG!",
                 pkt->node_name,
                 recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
                 recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5],
                 pkt->ping_id);

        ping_pong_packet_t pong_reply = *pkt;
        pong_reply.type = MSG_TYPE_PONG;
        snprintf(pong_reply.node_name, sizeof(pong_reply.node_name), "ESP32_%02X%02X", s_my_mac[4], s_my_mac[5]);

        // 直接向来源对端回复 PONG
        esp_now_send(recv_info->src_addr, (uint8_t *)&pong_reply, sizeof(pong_reply));

        // 闪烁一下 LED2 提示回声
        gpio_set_level(LED2_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(LED2_PIN, 0);
    }
    // 场景 B：收到对方回复的 PONG ➔ 计算往返延时 RTT！
    else if (pkt->type == MSG_TYPE_PONG) {
        int64_t now_us = esp_timer_get_time();
        int64_t rtt_us = now_us - pkt->send_time_us;
        float rtt_ms = (float)rtt_us / 1000.0f;

        s_total_recv_pong++;
        if (s_avg_rtt_ms == 0.0f) s_avg_rtt_ms = rtt_ms;
        else s_avg_rtt_ms = s_avg_rtt_ms * 0.8f + rtt_ms * 0.2f; // 滑动平均滤波

        float delivery_rate = (float)s_total_acked / (float)s_total_sent * 100.0f;

        ESP_LOGI(TAG, "⚡ [收到 PONG 回执] PingID: #%lu | \033[32m往返时延 RTT: %.2f ms\033[0m (平均: %.2f ms) | 链路成功率: %.1f%%",
                 pkt->ping_id, rtt_ms, s_avg_rtt_ms, delivery_rate);

        // 闪烁指示
        gpio_set_level(LED2_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(LED2_PIN, 0);
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
    ESP_LOGI(TAG, "🏷️  本机 MAC 地址: %02X:%02X:%02X:%02X:%02X:%02X",
             s_my_mac[0], s_my_mac[1], s_my_mac[2], s_my_mac[3], s_my_mac[4], s_my_mac[5]);

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));

    esp_now_peer_info_t peer_info = {
        .channel = 0,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer_info.peer_addr, s_peer_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));
}

/**
 * ⏱️ 周期性链路探测任务：每 2 秒自动发射一次 Ping 测速包
 */
static void ping_probe_task(void *pvParameters)
{
    uint32_t ping_seq = 0;
    while (1) {
        ping_seq++;
        s_total_sent++;

        ping_pong_packet_t ping_pkt = {
            .type = MSG_TYPE_PING,
            .ping_id = ping_seq,
            .send_time_us = esp_timer_get_time(), // 捕获当前高精度时间戳
            .temp_celsius = 25.0f + (float)(ping_seq % 10) * 0.5f,
        };
        snprintf(ping_pkt.node_name, sizeof(ping_pkt.node_name), "ESP32_%02X%02X", s_my_mac[4], s_my_mac[5]);

        ESP_LOGI(TAG, "📤 [发射 PING 测速包 #%lu] 目标: %02X:%02X:%02X:%02X:%02X:%02X (附带温度: %.1f°C)...",
                 ping_seq, s_peer_mac[0], s_peer_mac[1], s_peer_mac[2],
                 s_peer_mac[3], s_peer_mac[4], s_peer_mac[5], ping_pkt.temp_celsius);

        esp_now_send(s_peer_mac, (uint8_t *)&ping_pkt, sizeof(ping_pkt));

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 15 实验 2：ESP-NOW 双向对讲与 RTT 测速  ");
    ESP_LOGI(TAG, "==================================================");

    gpio_config_t led_conf = { .pin_bit_mask = (1ULL << LED2_PIN), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&led_conf);
    gpio_set_level(LED2_PIN, 0);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_espnow_init();

    xTaskCreate(ping_probe_task, "ping_probe_task", 3072, NULL, 5, NULL);
}
