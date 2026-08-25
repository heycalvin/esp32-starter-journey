#include "web_server.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "bsp_sensor.h"
#include "bsp_led.h"
#include "sys_ota.h"
#include "sys_fsm.h"
#include "net_manager.h"
#include <ctype.h>

static const char *TAG = "WEB_SERVER";
static httpd_handle_t s_server = NULL;

/* 📱 强制门户配网 HTML 页面 (支持自动拉取空中 Wi-Fi 与下拉选择) */
static const char HTML_PORTAL_PAGE[] = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<title>ESP32 智能中控配网</title>"
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

static const char HTML_SAVE_SUCCESS[] =
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<title>配置已保存</title>"
"<style>body{background:#0f172a;color:#f8fafc;font-family:-apple-system,sans-serif;padding:40px 20px;text-align:center;}"
".card{background:#1e293b;border-radius:16px;padding:30px;max-width:360px;margin:0 auto;box-shadow:0 10px 30px rgba(0,0,0,0.5);}"
"h2{color:#10b981;}</style></head><body>"
"<div class='card'><h2>🎉 Wi-Fi 凭证已保存！</h2>"
"<p>ESP32 正在自动连接您的路由器并校准时间...</p>"
"<p style='color:#94a3b8;font-size:13px;'>请观察中控台屏幕，设备将自动进入正常工作状态。</p></div>"
"</body></html>";

/* URL 解码辅助函数 */
static void url_decode(char *dst, const char *src, size_t dst_len)
{
    char a, b;
    size_t written = 0;
    while (*src && written < dst_len - 1) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
        written++;
    }
    *dst = '\0';
}

/* 1. 根路径或强制门户检测 ➔ 返回配网页面 */
static esp_err_t portal_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, HTML_PORTAL_PAGE, HTTPD_RESP_USE_STRLEN);
}

/* 2. 扫描周围 Wi-Fi 列表 JSON API */
static esp_err_t scan_get_handler(httpd_req_t *req)
{
    wifi_scan_config_t scan_config = { .show_hidden = false };
    esp_wifi_scan_start(&scan_config, true);

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count > 10) ap_count = 10; // 最多返回前 10 个信号最好的 AP

    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (ap_records) {
        esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    }

    char *json = malloc(1024);
    if (!json) {
        if (ap_records) free(ap_records);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int offset = snprintf(json, 1024, "[");
    for (int i = 0; i < ap_count && offset < 950; i++) {
        if (strlen((char *)ap_records[i].ssid) == 0) continue;
        offset += snprintf(json + offset, 1024 - offset,
                           "%s{\"ssid\":\"%s\",\"rssi\":%d}",
                           (i > 0) ? "," : "", (char *)ap_records[i].ssid, ap_records[i].rssi);
    }
    snprintf(json + offset, 1024 - offset, "]");

    if (ap_records) free(ap_records);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ret;
}

/* 3. 保存配网表单 POST /save */
static esp_err_t save_post_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char raw_ssid[64] = {0};
    char raw_pass[64] = {0};
    char ssid[33] = {0};
    char pass[65] = {0};

    char *p_ssid = strstr(buf, "ssid=");
    if (p_ssid) {
        p_ssid += 5;
        char *p_end = strchr(p_ssid, '&');
        if (p_end) {
            strncpy(raw_ssid, p_ssid, p_end - p_ssid);
        } else {
            strncpy(raw_ssid, p_ssid, sizeof(raw_ssid) - 1);
        }
    }

    char *p_pass = strstr(buf, "password=");
    if (p_pass) {
        p_pass += 9;
        char *p_end = strchr(p_pass, '&');
        if (p_end) {
            strncpy(raw_pass, p_pass, p_end - p_pass);
        } else {
            strncpy(raw_pass, p_pass, sizeof(raw_pass) - 1);
        }
    }

    url_decode(ssid, raw_ssid, sizeof(ssid));
    url_decode(pass, raw_pass, sizeof(pass));

    ESP_LOGI(TAG, "📥 收到用户配网数据: SSID=[%s], Password=[%s]", ssid, pass);
    net_manager_save_credentials(ssid, pass);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_SAVE_SUCCESS, HTTPD_RESP_USE_STRLEN);

    // 延时 1 秒后自动重启接入路由器
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    bsp_sensor_data_t data;
    bsp_sensor_read_all(&data);

    char json_response[256];
    snprintf(json_response, sizeof(json_response),
             "{\"temp\":%.1f,\"humi\":%.1f,\"dist\":%.1f,\"pir\":%d,\"heap\":%lu,\"psram\":%lu,\"led\":%d}",
             data.ntc_temperature, data.dht_humidity, data.ultrasonic_dist_cm,
             data.pir_motion_detected ? 1 : 0, (unsigned long)data.free_heap_bytes,
             (unsigned long)data.free_psram_bytes, bsp_led_get_state() ? 1 : 0);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t control_post_handler(httpd_req_t *req)
{
    char buf[64];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = '\0';
        if (strstr(buf, "TOGGLE_LED") || strstr(buf, "toggle")) {
            bsp_led_toggle();
        }
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, "{\"result\":\"ok\"}");
}

static esp_err_t ota_post_handler(httpd_req_t *req)
{
    char buf[1024];
    int remaining = req->content_len;
    ESP_LOGI(TAG, "📦 收到 Web OTA 固件上传请求 (总大小: %d 字节)", remaining);

    sys_fsm_handle_event(HUB_FSM_EVT_START_OTA, 0);
    esp_err_t err = sys_ota_begin_upgrade(remaining);
    if (err != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    while (remaining > 0) {
        int to_read = remaining < sizeof(buf) ? remaining : sizeof(buf);
        int received = httpd_req_recv(req, buf, to_read);
        if (received <= 0) {
            ESP_LOGE(TAG, "❌ 接收 OTA 固件数据中断");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        sys_ota_write_chunk(buf, received);
        remaining -= received;
    }

    httpd_resp_sendstr(req, "{\"status\":\"OTA_SUCCESS_REBOOTING\"}");
    sys_ota_finish_and_reboot();
    return ESP_OK;
}

esp_err_t web_server_init(void)
{
    if (s_server) return ESP_OK;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;
    config.stack_size = 8192;

    if (httpd_start(&s_server, &config) == ESP_OK) {
        // 强制门户配网路由
        httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = portal_get_handler };
        httpd_register_uri_handler(s_server, &root_uri);

        httpd_uri_t detect1_uri = { .uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = portal_get_handler };
        httpd_register_uri_handler(s_server, &detect1_uri);

        httpd_uri_t detect2_uri = { .uri = "/generate_204", .method = HTTP_GET, .handler = portal_get_handler };
        httpd_register_uri_handler(s_server, &detect2_uri);

        httpd_uri_t detect3_uri = { .uri = "/connecttest.txt", .method = HTTP_GET, .handler = portal_get_handler };
        httpd_register_uri_handler(s_server, &detect3_uri);

        httpd_uri_t scan_uri = { .uri = "/api/scan", .method = HTTP_GET, .handler = scan_get_handler };
        httpd_register_uri_handler(s_server, &scan_uri);

        httpd_uri_t save_uri = { .uri = "/save", .method = HTTP_POST, .handler = save_post_handler };
        httpd_register_uri_handler(s_server, &save_uri);

        // 中控状态与 OTA
        httpd_uri_t status_uri = { .uri = "/api/status", .method = HTTP_GET, .handler = status_get_handler };
        httpd_register_uri_handler(s_server, &status_uri);

        httpd_uri_t ctrl_uri = { .uri = "/api/control", .method = HTTP_POST, .handler = control_post_handler };
        httpd_register_uri_handler(s_server, &ctrl_uri);

        httpd_uri_t ota_uri = { .uri = "/api/ota", .method = HTTP_POST, .handler = ota_post_handler };
        httpd_register_uri_handler(s_server, &ota_uri);

        ESP_LOGI(TAG, "🌐 [服务层] 嵌入式 REST WebServer + Captive Portal 启动 (端口: 80)！");
        return ESP_OK;
    }
    return ESP_FAIL;
}

void web_server_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
}
