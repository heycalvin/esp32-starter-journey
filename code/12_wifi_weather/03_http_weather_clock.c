/**
 * 第 12 关实验 3：HTTP 天气请求、JSON 解析与网络时钟
 *
 * 这个教学示例使用 HTTP 来专注理解“一问一答”和 JSON。真实产品应改用
 * HTTPS 并启用服务器证书校验，不能把明文 HTTP 当作生产方案。
 */

#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "cJSON.h"
#include "nvs_flash.h"

static const char *TAG = "WEATHER_CLOCK";

#define EXAMPLE_WIFI_SSID      "YOUR_WIFI_SSID"
#define EXAMPLE_WIFI_PASS      "YOUR_WIFI_PASSWORD"
#define EXAMPLE_WIFI_MAX_RETRY 5

// 北京坐标；逗号编码为 %2C，含义仍是请求三个“当前”天气字段。
#define WEATHER_API_URL \
    "http://api.open-meteo.com/v1/forecast?latitude=39.9042&longitude=116.4074" \
    "&current=temperature_2m%2Cwind_speed_10m%2Cweather_code"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAILED_BIT    BIT1
#define MAX_HTTP_RECV_BUFFER 1024

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_count;
static char s_response_buffer[MAX_HTTP_RECV_BUFFER];
static size_t s_buffer_length;
static bool s_response_too_large;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_ERROR_CHECK(esp_wifi_connect());
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < EXAMPLE_WIFI_MAX_RETRY) {
            s_retry_count++;
            ESP_LOGW(TAG, "Wi-Fi 断开，正在第 %d/%d 次重连...",
                     s_retry_count, EXAMPLE_WIFI_MAX_RETRY);
            ESP_ERROR_CHECK(esp_wifi_connect());
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAILED_BIT);
            ESP_LOGE(TAG, "Wi-Fi 重连失败，无法更新天气。");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "已拿到 IP，可以访问网络服务。");
    }
}

static bool wifi_connect(void)
{
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        return false;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAILED_BIT,
        pdFALSE, pdFALSE, portMAX_DELAY);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

static void sync_time(void)
{
    setenv("TZ", "CST-8", 1);
    tzset();

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");
    ESP_ERROR_CHECK(esp_netif_sntp_init(&config));
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) == ESP_OK) {
        ESP_LOGI(TAG, "网络时间已同步。");
    } else {
        ESP_LOGW(TAG, "网络授时超时；天气请求仍会继续。 ");
    }
}

static esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    if (event->event_id != HTTP_EVENT_ON_DATA || s_response_too_large) {
        return ESP_OK;
    }

    size_t free_space = sizeof(s_response_buffer) - s_buffer_length - 1;
    if ((size_t)event->data_len > free_space) {
        s_response_too_large = true;
        ESP_LOGE(TAG, "天气响应超过 %u 字节缓冲区，已放弃本次解析。",
                 (unsigned int)(sizeof(s_response_buffer) - 1));
        return ESP_FAIL;
    }

    memcpy(s_response_buffer + s_buffer_length, event->data, event->data_len);
    s_buffer_length += event->data_len;
    s_response_buffer[s_buffer_length] = '\0';
    return ESP_OK;
}

static const char *weather_code_to_text(int weather_code)
{
    switch (weather_code) {
        case 0: return "晴朗";
        case 1: case 2: case 3: return "多云";
        case 45: case 48: return "有雾";
        case 51: case 53: case 55: return "毛毛雨";
        case 61: case 63: case 65: return "下雨";
        case 71: case 73: case 75: return "下雪";
        case 80: case 81: case 82: return "阵雨";
        case 95: return "雷暴";
        default: return "其他天气";
    }
}

static void parse_weather_json(const char *json_text)
{
    cJSON *root = cJSON_Parse(json_text);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON 解析失败：收到的不是完整有效的 JSON。");
        return;
    }

    cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
    cJSON *temperature = current == NULL ? NULL :
        cJSON_GetObjectItemCaseSensitive(current, "temperature_2m");
    cJSON *wind_speed = current == NULL ? NULL :
        cJSON_GetObjectItemCaseSensitive(current, "wind_speed_10m");
    cJSON *weather_code = current == NULL ? NULL :
        cJSON_GetObjectItemCaseSensitive(current, "weather_code");

    if (!cJSON_IsNumber(temperature) || !cJSON_IsNumber(wind_speed) ||
        !cJSON_IsNumber(weather_code)) {
        ESP_LOGE(TAG, "JSON 字段不完整或类型不对，本次不显示天气。 ");
        cJSON_Delete(root);
        return;
    }

    ESP_LOGI(TAG, "北京当前天气：%s，气温 %.1f °C，风速 %.1f km/h",
             weather_code_to_text(weather_code->valueint),
             temperature->valuedouble, wind_speed->valuedouble);
    cJSON_Delete(root);
}

static void fetch_weather_task(void *arg)
{
    while (true) {
        memset(s_response_buffer, 0, sizeof(s_response_buffer));
        s_buffer_length = 0;
        s_response_too_large = false;

        esp_http_client_config_t config = {
            .url = WEATHER_API_URL,
            .event_handler = http_event_handler,
            .timeout_ms = 5000,
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client == NULL) {
            ESP_LOGE(TAG, "无法创建 HTTP 客户端。");
        } else {
            esp_err_t err = esp_http_client_perform(client);
            int status_code = esp_http_client_get_status_code(client);

            if (s_response_too_large) {
                ESP_LOGW(TAG, "响应过大，本轮天气更新结束。");
            } else if (err != ESP_OK) {
                ESP_LOGE(TAG, "HTTP 请求失败：%s", esp_err_to_name(err));
            } else if (status_code != 200) {
                ESP_LOGW(TAG, "服务器返回 HTTP %d，本轮不解析。", status_code);
            } else {
                parse_weather_json(s_response_buffer);
            }
            esp_http_client_cleanup(client);
        }

        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "第 12 关实验 3：HTTP 天气时钟");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (!wifi_connect()) {
        ESP_LOGE(TAG, "没有拿到 IP，停止天气实验。");
        return;
    }
    sync_time();

    xTaskCreate(fetch_weather_task, "weather_task", 4096, NULL, 5, NULL);

    while (true) {
        time_t now;
        struct tm timeinfo;
        char time_text[48];

        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(time_text, sizeof(time_text), "%Y-%m-%d %H:%M:%S", &timeinfo);
        ESP_LOGI(TAG, "北京时间：%s", time_text);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
