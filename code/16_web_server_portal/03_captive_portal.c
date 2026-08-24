/**
 * 🌟 ESP32 物联网实战 —— 第 16 关 实验 3：商业级全生命周期综合大工程
 *    【APSTA 空中扫网 + 自动弹窗下拉选网 + NVS 存储 + mDNS 网页中控 + 长按 SW3 重置】
 * 
 * 🎯 完整商业级产品闭环：
 *    1. 【APSTA 混合模式】：一边发射 ESP32-Setup-WiFi 热点，一边后台扫描周围 Wi-Fi 列表；
 *    2. 【自动弹窗 + 下拉选网】：手机连上后自动弹窗，网页自动拉取空中 Wi-Fi 列表供用户下拉点选；
 *    3. 【凭证持久化】：用户只需点选 Wi-Fi 并输入密码，ESP32 存入 NVS 闪存并自动重启；
 *    4. 【联网中控】：重启后自动连上路由器，启动 WebServer + mDNS (http://esp32.local)；
 *    5. 【网页控灯】：手机/电脑打开 http://esp32.local 实时开关 LED2；
 *    6. 【出厂重置】：长按板载 SW3 按键 3 秒 ➔ 擦除 NVS 配网并重启重回配网模式！
 * 
 * 📌 硬件引脚分配：
 *    - 板载 LED2：     GPIO27 (配网模式慢闪，已联网常亮)
 *    - 用户按键 SW3：  GPIO39 (长按 3 秒清除配网并重启)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "mdns.h"
#include "lwip/sockets.h"

static const char *TAG = "EXP3_FULL_PORTAL_HUB";

#define LED2_PIN        GPIO_NUM_27
#define BUTTON_PIN      GPIO_NUM_39

#define AP_SSID         "ESP32-Setup-WiFi"
#define MDNS_HOSTNAME   "esp32"
#define NVS_NAMESPACE   "wifi_store"

typedef enum {
    MODE_PROVISIONING_AP, // 阶段 1：强制门户配网模式 (开热点等待用户输入密码)
    MODE_CONNECTED_STA    // 阶段 2：联网中控模式 (已连路由器，开启网页中控)
} system_mode_t;

static system_mode_t s_mode = MODE_PROVISIONING_AP;
static bool s_led_state = false;
static httpd_handle_t s_server = NULL;
static int s_dns_socket = -1;

/* 📱 阶段 1 配网页面 (带 Wi-Fi 列表自动拉取与下拉选择框) */
static const char HTML_PORTAL_PAGE[] = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<title>ESP32 智能设备配网</title>"
"<style>"
"body{background:#0f172a;color:#f8fafc;font-family:-apple-system,sans-serif;margin:0;padding:20px;display:flex;justify-content:center;align-items:center;min-height:90vh;}"
".card{background:#1e293b;border:1px solid #334155;border-radius:20px;padding:28px;width:100%;max-width:380px;text-align:center;box-shadow:0 20px 40px rgba(0,0,0,0.5);}"
"h2{color:#38bdf8;margin:0 0 8px 0;}"
"p{color:#94a3b8;font-size:14px;margin-bottom:20px;}"
"label{font-size:13px;color:#94a3b8;margin:12px 0 6px 0;display:block;text-align:left;font-weight:600;}"
"select,input{width:100%;box-sizing:border-box;background:#0f172a;border:1px solid #475569;color:#fff;padding:12px;border-radius:10px;font-size:15px;outline:none;margin-bottom:8px;}"
"select:focus,input:focus{border-color:#38bdf8;}"
".btn{width:100%;background:#0ea5e9;color:#fff;border:none;padding:14px;font-size:16px;font-weight:bold;border-radius:10px;cursor:pointer;margin-top:16px;}"
".btn:hover{background:#0284c7;}"
".refresh{font-size:12px;color:#38bdf8;cursor:pointer;text-align:right;display:block;margin-bottom:4px;}"
"</style></head><body>"
"<div class='card'>"
"<h2>⚡ ESP32 智能开箱配网</h2>"
"<p>请点选您家中的 Wi-Fi 并输入密码</p>"
"<form action='/save' method='POST'>"
"<div style='display:flex;justify-content:space-between;align-items:center;'>"
"<label style='margin:0;'>周围 Wi-Fi 列表</label><span class='refresh' onclick='scanWiFi()'>🔄 刷新</span>"
"</div>"
"<select id='wifi_select' onchange='selectSSID()'><option value=''>📡 正在扫描周围 Wi-Fi...</option></select>"
"<label>Wi-Fi 名称 (SSID)</label><input id='ssid_input' name='ssid' placeholder='可下拉选择或手动输入' required>"
"<label>Wi-Fi 密码</label><input type='password' name='password' placeholder='请输入密码'>"
"<button type='submit' class='btn'>保存并连接网络</button>"
"</form></div>"
"<script>"
"function scanWiFi(){"
"  let s=document.getElementById('wifi_select'); s.innerHTML='<option>📡 正在扫描周围 Wi-Fi...</option>';"
"  fetch('/api/scan').then(r=>r.json()).then(list=>{"
"    s.innerHTML='<option value=\"\">-- 请在下方点选您的 Wi-Fi --</option>';"
"    list.forEach(w=>{"
"      let opt=document.createElement('option');"
"      opt.value=w.ssid; opt.innerText='📶 '+w.ssid+' ('+w.rssi+' dBm)';"
"      s.appendChild(opt);"
"    });"
"  }).catch(()=>{s.innerHTML='<option>⚠️ 扫描失败，请手动输入</option>';});"
"}"
"function selectSSID(){"
"  let v=document.getElementById('wifi_select').value;"
"  if(v) document.getElementById('ssid_input').value=v;"
"}"
"scanWiFi();"
"</script></body></html>";

/* 🌐 阶段 2 中控页面 (Dashboard HTML) */
static const char HTML_DASHBOARD_PAGE[] = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<title>ESP32 智能中控台</title>"
"<style>"
"body{background:#0d1117;color:#c9d1d9;font-family:-apple-system,sans-serif;text-align:center;padding:30px;}"
".card{background:#161b22;border:1px solid #30363d;border-radius:20px;max-width:400px;margin:0 auto;padding:26px;box-shadow:0 12px 30px rgba(0,0,0,0.6);}"
"h2{color:#58a6ff;margin:0 0 10px 0;}"
".domain{background:#21262d;color:#58a6ff;padding:6px 14px;border-radius:8px;font-family:monospace;font-size:15px;display:inline-block;margin:10px 0;}"
".btn{background:#238636;color:white;border:none;padding:14px 36px;font-size:18px;font-weight:bold;border-radius:30px;cursor:pointer;margin-top:20px;transition:0.2s;}"
".btn-off{background:#da3633;}"
".badge{display:inline-block;padding:4px 12px;border-radius:12px;background:#21262d;color:#3fb950;font-weight:bold;margin:10px 0;}"
"</style></head><body>"
"<div class='card'>"
"<h2>🌐 ESP32 网页智能中控</h2>"
"<div>访问域名: <span class='domain'>http://esp32.local</span></div>"
"<div>当前绿色 LED2: <span id='led_text' class='badge'>读取中...</span></div>"
"<div><button id='btn' class='btn' onclick='toggle()'>切换 LED 状态</button></div>"
"<p style='color:#8b949e;font-size:12px;margin-top:24px;'>提示: 长按开发板 SW3 按键 3 秒可恢复出厂配网模式</p>"
"</div>"
"<script>"
"function getSt(){fetch('/api/status').then(r=>r.json()).then(d=>{"
"  let s=d.led==1; document.getElementById('led_text').innerText=s?'💡 已点亮 (ON)':'🌑 已熄灭 (OFF)';"
"  document.getElementById('led_text').style.color=s?'#3fb950':'#f85149';"
"  let b=document.getElementById('btn'); b.innerText=s?'关 闭 LED2':'点 亮 LED2'; b.className=s?'btn btn-off':'btn';"
"});}"
"function toggle(){fetch('/api/led').then(()=>getSt());}"
"getSt(); setInterval(getSt, 3000);"
"</script></body></html>";

/* ⏳ 阶段 1 ➔ 阶段 2 过渡页面 (5 秒倒计时自动跳转至 http://esp32.local) */
static const char HTML_SAVED_PAGE[] = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<meta http-equiv='refresh' content='6;url=http://esp32.local/'>"
"<title>配网成功 - 正在跳转</title>"
"<style>"
"body{background:#0f172a;color:#f8fafc;font-family:-apple-system,sans-serif;margin:0;padding:20px;display:flex;justify-content:center;align-items:center;min-height:90vh;}"
".card{background:#1e293b;border:1px solid #334155;border-radius:20px;padding:32px;width:100%;max-width:380px;text-align:center;box-shadow:0 20px 40px rgba(0,0,0,0.5);}"
"h2{color:#38bdf8;margin:0 0 12px 0;}"
"p{color:#94a3b8;font-size:14px;line-height:1.6;}"
".timer{font-size:36px;font-weight:bold;color:#38bdf8;margin:20px 0;}"
".btn{display:inline-block;background:#0ea5e9;color:#fff;text-decoration:none;padding:12px 28px;font-size:15px;font-weight:bold;border-radius:25px;margin-top:10px;transition:0.2s;}"
".btn:hover{background:#0284c7;}"
"</style></head><body>"
"<div class='card'>"
"<h2>🎉 Wi-Fi 配置已保存！</h2>"
"<p>ESP32 正在自动连入您的路由器<br>请确保手机已切回家庭 Wi-Fi</p>"
"<div class='timer'><span id='cnt'>5</span>s</div>"
"<p>倒计时结束后将自动跳转至中控台...</p>"
"<a href='http://esp32.local/' class='btn'>立即前往中控台 ➔</a>"
"</div>"
"<script>"
"let sec = 5;"
"let t = setInterval(()=>{"
"  sec--;"
"  if(sec >= 0) document.getElementById('cnt').innerText = sec;"
"  if(sec <= 0){ clearInterval(t); location.href='http://esp32.local/'; }"
"}, 1000);"
"</script></body></html>";

/* 🌐 路由 1: GET / (统一主页路由) */
static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    if (s_mode == MODE_PROVISIONING_AP) {
        return httpd_resp_send(req, HTML_PORTAL_PAGE, HTTPD_RESP_USE_STRLEN);
    } else {
        return httpd_resp_send(req, HTML_DASHBOARD_PAGE, HTTPD_RESP_USE_STRLEN);
    }
}

/* 🌐 路由 2: GET /api/scan (扫描空中 Wi-Fi 并返回 JSON 列表) */
static esp_err_t scan_get_handler(httpd_req_t *req)
{
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false
    };

    ESP_LOGI(TAG, "📡 正在扫描周围 2.4GHz Wi-Fi 热点...");
    esp_wifi_scan_start(&scan_config, true); // 阻塞扫描

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count > 15) ap_count = 15; // 最多展示前 15 个强信号

    wifi_ap_record_t *ap_records = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);
    char *json_buf = (char *)malloc(2048);
    strcpy(json_buf, "[");

    if (ap_records && json_buf) {
        esp_wifi_scan_get_ap_records(&ap_count, ap_records);
        int valid_count = 0;
        for (int i = 0; i < ap_count; i++) {
            if (strlen((char *)ap_records[i].ssid) == 0) continue;
            char item[128];
            snprintf(item, sizeof(item), "%s{\"ssid\":\"%s\",\"rssi\":%d}",
                     valid_count > 0 ? "," : "",
                     (char *)ap_records[i].ssid, ap_records[i].rssi);
            strcat(json_buf, item);
            valid_count++;
        }
        strcat(json_buf, "]");
        ESP_LOGI(TAG, "✅ 扫描完成，共找到 %d 个有效 Wi-Fi 热点", valid_count);
    } else {
        strcpy(json_buf, "[]");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_buf, HTTPD_RESP_USE_STRLEN);

    if (ap_records) free(ap_records);
    if (json_buf) free(json_buf);
    return ESP_OK;
}

/* 🌐 路由 3: POST /save (保存配网凭证到 NVS) */
static esp_err_t save_post_handler(httpd_req_t *req)
{
    char buf[256] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;

    char ssid[33] = {0}, password[65] = {0};
    char *ssid_ptr = strstr(buf, "ssid="), *pass_ptr = strstr(buf, "password=");
    if (ssid_ptr) sscanf(ssid_ptr, "ssid=%32[^&]", ssid);
    if (pass_ptr) sscanf(pass_ptr, "password=%64s", password);

    ESP_LOGI(TAG, "💾 [收到配网凭证] SSID: \033[32m%s\033[0m, Password: \033[33m%s\033[0m", ssid, password);

    nvs_handle_t nvs_handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
        nvs_set_str(nvs_handle, "ssid", ssid);
        nvs_set_str(nvs_handle, "password", password);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "✅ Wi-Fi 凭证已存入 NVS 闪存！2 秒后重启连网...");
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, HTML_SAVED_PAGE, HTTPD_RESP_USE_STRLEN);

    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    char json_buf[128];
    uint32_t uptime_sec = (uint32_t)(esp_timer_get_time() / 1000000);
    snprintf(json_buf, sizeof(json_buf), 
             "{\"led\":%d,\"free_heap\":%lu,\"uptime_sec\":%lu}",
             s_led_state ? 1 : 0, (unsigned long)esp_get_free_heap_size(), (unsigned long)uptime_sec);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_buf, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t led_control_handler(httpd_req_t *req)
{
    s_led_state = !s_led_state;
    gpio_set_level(LED2_PIN, s_led_state ? 1 : 0);
    ESP_LOGI(TAG, "💡 [网页指令] 翻转 LED2 ➔ %s", s_led_state ? "点亮" : "熄灭");
    return httpd_resp_send(req, "OK", 2);
}

static esp_err_t redirect_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, NULL, 0);
}

static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 7;
    config.lru_purge_enable = true;

    if (httpd_start(&s_server, &config) == ESP_OK) {
        httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_get_handler };
        httpd_register_uri_handler(s_server, &index_uri);

        httpd_uri_t scan_uri = { .uri = "/api/scan", .method = HTTP_GET, .handler = scan_get_handler };
        httpd_register_uri_handler(s_server, &scan_uri);

        httpd_uri_t save_uri = { .uri = "/save", .method = HTTP_POST, .handler = save_post_handler };
        httpd_register_uri_handler(s_server, &save_uri);

        httpd_uri_t status_uri = { .uri = "/api/status", .method = HTTP_GET, .handler = status_get_handler };
        httpd_register_uri_handler(s_server, &status_uri);

        httpd_uri_t led_uri = { .uri = "/api/led", .method = HTTP_GET, .handler = led_control_handler };
        httpd_register_uri_handler(s_server, &led_uri);

        httpd_uri_t apple_uri = { .uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = redirect_handler };
        httpd_register_uri_handler(s_server, &apple_uri);

        httpd_uri_t android_uri = { .uri = "/generate_204", .method = HTTP_GET, .handler = redirect_handler };
        httpd_register_uri_handler(s_server, &android_uri);
    }
}

/* 📡 DNS 劫持任务 */
static void dns_hijack_server_task(void *pvParameters)
{
    uint8_t rx_buf[128];
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    s_dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    bind(s_dns_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int len = recvfrom(s_dns_socket, rx_buf, sizeof(rx_buf), 0, (struct sockaddr *)&client_addr, &client_addr_len);

        if (len > 12) {
            rx_buf[2] |= 0x80; rx_buf[3] |= 0x80; rx_buf[7] = 1;
            int idx = len;
            rx_buf[idx++] = 0xC0; rx_buf[idx++] = 0x0C;
            rx_buf[idx++] = 0x00; rx_buf[idx++] = 0x01;
            rx_buf[idx++] = 0x00; rx_buf[idx++] = 0x01;
            rx_buf[idx++] = 0x00; rx_buf[idx++] = 0x00; rx_buf[idx++] = 0x00; rx_buf[idx++] = 0x3C;
            rx_buf[idx++] = 0x00; rx_buf[idx++] = 0x04;
            rx_buf[idx++] = 192;  rx_buf[idx++] = 168;  rx_buf[idx++] = 4;   rx_buf[idx++] = 1;
            sendto(s_dns_socket, rx_buf, idx, 0, (struct sockaddr *)&client_addr, client_addr_len);
        }
    }
}

static void start_ap_provisioning(void)
{
    s_mode = MODE_PROVISIONING_AP;
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta(); // ⭐️ 开启 STA netif 以支持空中扫描

    wifi_config_t ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    // ⭐️ 核心关键：开启 APSTA 混合模式，一边开热点，一边扫 Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "📶  未检测到可用 Wi-Fi 凭证，进入【开箱配网模式】！");
    ESP_LOGI(TAG, "📲  请用手机连接热点: \033[36m%s\033[0m", AP_SSID);
    ESP_LOGI(TAG, "🌐  手机连上后将自动弹窗并展示周围可选择的 Wi-Fi！");
    ESP_LOGI(TAG, "==================================================");

    start_webserver();
    xTaskCreate(dns_hijack_server_task, "dns_hijack", 3072, NULL, 5, NULL);
}

static void start_sta_connected(const char *ssid, const char *pass)
{
    s_mode = MODE_CONNECTED_STA;
    esp_netif_create_default_wifi_sta();

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "🔄 正在连接家庭 Wi-Fi: \033[32m%s\033[0m...", ssid);
    esp_wifi_connect();
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "==================================================");
        ESP_LOGI(TAG, "🎉 Wi-Fi 连接成功！物理 IP: " IPSTR, IP2STR(&event->ip_info.ip));

        start_webserver();

        ESP_ERROR_CHECK(mdns_init());
        ESP_ERROR_CHECK(mdns_hostname_set(MDNS_HOSTNAME));
        mdns_service_add("ESP32-Web-Server", "_http", "_tcp", 80, NULL, 0);

        ESP_LOGI(TAG, "🌐 请打开电脑/手机浏览器访问: \033[32mhttp://%s.local\033[0m", MDNS_HOSTNAME);
        ESP_LOGI(TAG, "==================================================");

        gpio_set_level(LED2_PIN, 1);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_mode == MODE_CONNECTED_STA) {
            ESP_LOGW(TAG, "⚠️ Wi-Fi 连接断开，尝试重新连接...");
            esp_wifi_connect();
        }
    }
}

static void factory_reset_button_task(void *pvParameters)
{
    int press_ms = 0;
    while (1) {
        if (gpio_get_level(BUTTON_PIN) == 0) {
            press_ms += 100;
            if (press_ms >= 3000) {
                ESP_LOGW(TAG, "⚠️ [长按 3 秒触发] 正在清除 NVS Wi-Fi 配网凭证...");
                nvs_handle_t nvs_handle;
                if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
                    nvs_erase_all(nvs_handle);
                    nvs_commit(nvs_handle);
                    nvs_close(nvs_handle);
                }
                for (int i = 0; i < 5; i++) {
                    gpio_set_level(LED2_PIN, 1);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    gpio_set_level(LED2_PIN, 0);
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                ESP_LOGI(TAG, "🔄 恢复出厂设置成功！正在重启重新进入配网模式...");
                esp_restart();
            }
        } else {
            press_ms = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 16 实验 3：全生命周期配网与网页中控大成  ");
    ESP_LOGI(TAG, "==================================================");

    gpio_config_t io_conf = { .pin_bit_mask = (1ULL << LED2_PIN), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io_conf);
    gpio_set_level(LED2_PIN, 0);

    gpio_config_t btn_conf = { .pin_bit_mask = (1ULL << BUTTON_PIN), .mode = GPIO_MODE_INPUT };
    gpio_config(&btn_conf);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    nvs_handle_t nvs_handle;
    char saved_ssid[33] = {0}, saved_pass[65] = {0};
    size_t ssid_len = sizeof(saved_ssid), pass_len = sizeof(saved_pass);
    bool has_wifi = false;

    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) == ESP_OK) {
        if (nvs_get_str(nvs_handle, "ssid", saved_ssid, &ssid_len) == ESP_OK) {
            nvs_get_str(nvs_handle, "password", saved_pass, &pass_len);
            has_wifi = true;
            ESP_LOGI(TAG, "📦 [NVS 读取成功] 找到已存 Wi-Fi: \033[32m%s\033[0m", saved_ssid);
        }
        nvs_close(nvs_handle);
    }

    if (has_wifi) {
        start_sta_connected(saved_ssid, saved_pass);
    } else {
        start_ap_provisioning();
    }

    xTaskCreate(factory_reset_button_task, "reset_btn", 3072, NULL, 5, NULL);
}
