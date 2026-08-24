/**
 * 🌟 ESP32 物联网实战 —— 第 16 关 实验 2：mDNS 本地域名解析与零配置直达 (http://esp32.local)
 * 
 * 🎯 学习目标：
 *    1. 理解 mDNS（Multicast DNS，多播 DNS）局域网服务发现协议底层原理；
 *    2. 掌握 ESP-IDF 原生 mdns 组件的初始化与主机名绑定（mdns_hostname_set）；
 *    3. 在局域网中注册 _http._tcp 服务广播；
 *    4. 彻底告别繁琐的 IP 地址查找，电脑/手机浏览器直接输入 http://esp32.local 秒级直达！
 * 
 * 📌 硬件引脚分配：
 *    - 板载 LED2：     GPIO27 (高电平点亮)
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
#include "esp_event.h"
#include "esp_http_server.h"
#include "mdns.h"

static const char *TAG = "EXP2_MDNS_SERVER";

#define LED2_PIN        GPIO_NUM_27
#define MDNS_HOSTNAME   "esp32"     // 👉 局域网访问域名: http://esp32.local

/* 请修改为你家里的路由器 Wi-Fi 账号密码 */
#define WIFI_SSID       "TP-LINK_Test"
#define WIFI_PASS       "12345678"

static bool s_led_state = false;
static httpd_handle_t s_server = NULL;

static const char HTML_MDNS_PAGE[] = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<title>ESP32 mDNS 智能中控</title>"
"<style>"
"body{background:#0b0f19;color:#e2e8f0;font-family:-apple-system,sans-serif;text-align:center;padding:40px;}"
".box{background:#1e293b;border:1px solid #334155;border-radius:20px;max-width:420px;margin:0 auto;padding:30px;box-shadow:0 10px 30px rgba(0,0,0,0.6);}"
"h2{color:#38bdf8;margin:0 0 10px 0;}"
".domain{background:#0f172a;color:#38bdf8;padding:8px 16px;border-radius:8px;font-family:monospace;font-size:16px;display:inline-block;margin:15px 0;}"
".btn{background:#0ea5e9;color:white;border:none;padding:14px 36px;font-size:18px;font-weight:bold;border-radius:30px;cursor:pointer;transition:0.2s;margin-top:20px;}"
".btn:hover{background:#0284c7;transform:translateY(-2px);}"
".state{font-size:18px;margin:15px 0;font-weight:bold;}"
"</style></head><body>"
"<div class='box'>"
"<h2>🌐 mDNS 域名直达中控台</h2>"
"<div>当前访问域名:</div>"
"<div class='domain'>http://esp32.local</div>"
"<div class='state'>LED2 状态: <span id='st' style='color:#38bdf8'>读取中...</span></div>"
"<button class='btn' onclick='toggle()'>翻转 LED2 状态</button>"
"</div>"
"<script>"
"function getSt(){fetch('/api/status').then(r=>r.json()).then(d=>{document.getElementById('st').innerText=d.led?'💡 点亮 (ON)':'🌑 熄灭 (OFF)';});}"
"function toggle(){fetch('/api/led').then(()=>getSt());}"
"getSt();"
"</script></body></html>";

static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, HTML_MDNS_PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"led\":%d}", s_led_state ? 1 : 0);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t led_get_handler(httpd_req_t *req)
{
    s_led_state = !s_led_state;
    gpio_set_level(LED2_PIN, s_led_state ? 1 : 0);
    ESP_LOGI(TAG, "💡 域名网页控制 ➔ 翻转 LED2 状态为: %s", s_led_state ? "点亮" : "熄灭");
    return httpd_resp_send(req, "OK", 2);
}

static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&s_server, &config) == ESP_OK) {
        httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_get_handler };
        httpd_register_uri_handler(s_server, &index_uri);
        httpd_uri_t st_uri = { .uri = "/api/status", .method = HTTP_GET, .handler = status_get_handler };
        httpd_register_uri_handler(s_server, &st_uri);
        httpd_uri_t led_uri = { .uri = "/api/led", .method = HTTP_GET, .handler = led_get_handler };
        httpd_register_uri_handler(s_server, &led_uri);
    }
}

/**
 * 🏷️ 初始化 mDNS 局域网域名解析服务
 */
static void init_mdns(void)
{
    ESP_ERROR_CHECK(mdns_init());
    
    // 设置主机名为 "esp32" ➔ 在局域网中通过 http://esp32.local 访问
    ESP_ERROR_CHECK(mdns_hostname_set(MDNS_HOSTNAME));
    ESP_LOGI(TAG, "🏷️  mDNS 主机名已绑定: \033[36m%s.local\033[0m", MDNS_HOSTNAME);

    // 设置设备实例名称
    ESP_ERROR_CHECK(mdns_instance_name_set("ESP32 IoT Smart Station"));

    // 注册 HTTP Web 服务广播 (让苹果 Bonjour/局域网服务嗅探器自动发现)
    mdns_service_add("ESP32-Web-Server", "_http", "_tcp", 80, NULL, 0);

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "🎉 mDNS 服务启动成功！");
    ESP_LOGI(TAG, "🌐 浏览器可免查 IP，直接输入: \033[32mhttp://%s.local\033[0m 直达控制面板！", MDNS_HOSTNAME);
    ESP_LOGI(TAG, "==================================================");
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "🎉 Wi-Fi 已连接！分配物理 IP: " IPSTR, IP2STR(&event->ip_info.ip));

        // 启动 WebServer 与 mDNS 域名服务
        if (s_server == NULL) start_webserver();
        init_mdns();
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
        .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 16 实验 2：mDNS 本地域名直达 (esp32.local) ");
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
