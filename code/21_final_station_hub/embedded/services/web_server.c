#include "web_server.h"
#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "bsp_sensor.h"
#include "bsp_led.h"
#include "sys_ota.h"
#include "sys_fsm.h"

static const char *TAG = "WEB_SERVER";
static httpd_handle_t s_server = NULL;

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
    config.max_uri_handlers = 8;
    config.stack_size = 8192;

    if (httpd_start(&s_server, &config) == ESP_OK) {
        httpd_uri_t status_uri = {
            .uri = "/api/status",
            .method = HTTP_GET,
            .handler = status_get_handler,
        };
        httpd_register_uri_handler(s_server, &status_uri);

        httpd_uri_t ctrl_uri = {
            .uri = "/api/control",
            .method = HTTP_POST,
            .handler = control_post_handler,
        };
        httpd_register_uri_handler(s_server, &ctrl_uri);

        httpd_uri_t ota_uri = {
            .uri = "/api/ota",
            .method = HTTP_POST,
            .handler = ota_post_handler,
        };
        httpd_register_uri_handler(s_server, &ota_uri);

        ESP_LOGI(TAG, "🌐 [服务层] 嵌入式 REST WebServer 启动 (端口: 80, 支持 Web OTA 与 REST API)！");
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
