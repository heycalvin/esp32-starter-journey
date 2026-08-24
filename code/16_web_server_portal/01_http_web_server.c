/**
 * 🌟 ESP32 物联网实战 —— 第 16 关 实验 1：局域网微型 Web 服务器与网页控制面板 (Web Server)
 * 
 * 🎯 学习目标：
 *    1. 掌握 ESP-IDF 原生轻量级 HTTP 服务器框架 (esp_http_server)；
 *    2. 掌握 HTTP GET/POST 路由注册与 URI 处理器 (URI Handler)；
 *    3. 在 ESP32 内部嵌入富美学 HTML5 + CSS3 + RESTful API 控制面板；
 *    4. 手机/电脑浏览器访问板载 IP，点击网页按钮实时开关板载绿色 LED2！
 * 
 * 📌 硬件引脚分配：
 *    - 板载 LED2：     GPIO27 (高电平点亮)
 *    - 用户按键 SW3：  GPIO39 (输入专用)
 * 
 * ⚙️ 实验运行说明：
 *    - 请在下方配置你的路由器 Wi-Fi 账号与密码；
 *    - 烧录后查看串口输出的板载 IP 地址（例如 http://192.168.1.105）；
 *    - 同一 Wi-Fi 下的电脑或手机浏览器打开该 IP 即可体验网页中控！
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
#include "esp_event.h"
#include "esp_http_server.h"

static const char *TAG = "EXP1_WEB_SERVER";

#define LED2_PIN        GPIO_NUM_27

/* 请修改为你家里的路由器 Wi-Fi 账号密码 */
#define WIFI_SSID       "TP-LINK_Test"
#define WIFI_PASS       "12345678"

static bool s_led_state = false;
static httpd_handle_t s_server = NULL;

/* 嵌入式纯原生深色科技风 HTML/CSS 控制页面 */
static const char HTML_INDEX_PAGE[] = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<title>ESP32 智能中控台</title>"
"<style>"
"body{background:#0d1117;color:#c9d1d9;font-family:-apple-system,BlinkMacSystemFont,sans-serif;text-align:center;padding:30px;}"
".card{background:#161b22;border:1px solid #30363d;border-radius:16px;max-width:400px;margin:0 auto;padding:24px;box-shadow:0 8px 24px rgba(0,0,0,0.5);}"
"h2{color:#58a6ff;margin-top:0;}"
".btn{background:#238636;color:white;border:none;padding:14px 32px;font-size:18px;font-weight:bold;border-radius:30px;cursor:pointer;transition:0.2s;margin-top:20px;}"
".btn:hover{background:#2ea043;transform:scale(1.03);}"
".btn-off{background:#da3633;}"
".btn-off:hover{background:#f85149;}"
".status{font-size:16px;margin:15px 0;color:#8b949e;}"
".badge{display:inline-block;padding:4px 12px;border-radius:12px;background:#21262d;color:#58a6ff;font-weight:bold;}"
"</style></head><body>"
"<div class='card'>"
"<h2>⚡ ESP32 网页智能中控</h2>"
"<div class='status'>硬件平台: <span class='badge'>ESP32-D0WD-V3</span></div>"
"<div class='status'>当前绿色 LED2: <span id='led_text' class='badge'>读取中...</span></div>"
"<button id='toggle_btn' class='btn' onclick='toggleLED()'>切换 LED 状态</button>"
"</div>"
"<script>"
"function updateStatus(){"
"  fetch('/api/status').then(r=>r.json()).then(d=>{"
"    let s=d.led_state==1;"
"    document.getElementById('led_text').innerText=s?'💡 已点亮 (ON)':'🌑 已熄灭 (OFF)';"
"    document.getElementById('led_text').style.color=s?'#3fb950':'#f85149';"
"    let b=document.getElementById('toggle_btn');"
"    b.innerText=s?'关 闭 LED2':'点 亮 LED2';"
"    b.className=s?'btn btn-off':'btn';"
"  });"
"}"
"function toggleLED(){"
"  fetch('/api/led?toggle=1').then(()=>updateStatus());"
"}"
"updateStatus();"
"setInterval(updateStatus, 3000);"
"</script></body></html>";

/**
 * 🌐 路由 1: GET / (返回主页 HTML)
 */
static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, HTML_INDEX_PAGE, HTTPD_RESP_USE_STRLEN);
}

/**
 * 🌐 路由 2: GET /api/status (返回设备运行状态 JSON)
 */
static esp_err_t status_get_handler(httpd_req_t *req)
{
    char json_buf[128];
    uint32_t uptime_sec = (uint32_t)(esp_timer_get_time() / 1000000);
    snprintf(json_buf, sizeof(json_buf), 
             "{\"led_state\":%d,\"free_heap\":%lu,\"uptime_sec\":%lu}",
             s_led_state ? 1 : 0, (unsigned long)esp_get_free_heap_size(), (unsigned long)uptime_sec);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_buf, HTTPD_RESP_USE_STRLEN);
}

/**
 * 🌐 路由 3: GET /api/led (控制 LED 状态)
 */
static esp_err_t led_control_handler(httpd_req_t *req)
{
    s_led_state = !s_led_state;
    gpio_set_level(LED2_PIN, s_led_state ? 1 : 0);
    ESP_LOGI(TAG, "💡 [网页指令执行] 翻转板载 LED2 ➔ %s", s_led_state ? "点亮" : "熄灭");

    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "OK", 2);
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "🚀 正在启动轻量级 HTTP Web 服务器 (Port: %d)...", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // 注册 GET /
        httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_get_handler };
        httpd_register_uri_handler(server, &index_uri);

        // 注册 GET /api/status
        httpd_uri_t status_uri = { .uri = "/api/status", .method = HTTP_GET, .handler = status_get_handler };
        httpd_register_uri_handler(server, &status_uri);

        // 注册 GET /api/led
        httpd_uri_t led_uri = { .uri = "/api/led", .method = HTTP_GET, .handler = led_control_handler };
        httpd_register_uri_handler(server, &led_uri);

        return server;
    }
    return NULL;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "⚠️ Wi-Fi 断开连接，尝试重新连接...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "==================================================");
        ESP_LOGI(TAG, "🎉 Wi-Fi 连接成功！分配 IP: \033[32m" IPSTR "\033[0m", IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "🌐 请在电脑/手机浏览器访问: \033[36mhttp://" IPSTR "\033[0m", IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "==================================================");

        if (s_server == NULL) {
            s_server = start_webserver();
        }
    }
}

static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 16 实验 1：ESP32 局域网微型 Web 服务器   ");
    ESP_LOGI(TAG, "==================================================");

    gpio_config_t io_conf = { .pin_bit_mask = (1ULL << LED2_PIN), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io_conf);
    gpio_set_level(LED2_PIN, 0);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();
}
