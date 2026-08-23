/**
 * 🌟 ESP32 物联网实战 —— 第 12 关 实验 4：IP 自动定位 + HTTPS 安全加密天气时钟 (SSL/TLS 证书校验)
 * 
 * 🎯 学习目标：
 *    1. 搞懂 HTTP（明文）与 HTTPS（TLS/SSL 加密）的核心差异与握手流程；
 *    2. 掌握使用 `esp_crt_bundle_attach` 挂载 ESP-IDF 全球根证书包，安全校验权威 CA 机构证书；
 *    3. 第一阶段：通过 IP 定位接口自动获取当前设备所在城市的名称与经纬度；
 *    4. 第二阶段：通过 HTTPS 安全加密通道请求 Open-Meteo 天气 API，防御中间人窃听与数据篡改。
 */

#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "cJSON.h"
#include "nvs_flash.h"

static const char *TAG = "HTTPS_WEATHER";

// 请填写身边的 2.4 GHz Wi-Fi 账号密码
#define EXAMPLE_WIFI_SSID      "YOUR_WIFI_SSID"
#define EXAMPLE_WIFI_PASS      "YOUR_WIFI_PASSWORD"
#define EXAMPLE_WIFI_MAX_RETRY 5

// 📍 IP 地理位置查询接口
#define IP_GEO_API_URL         "http://ip-api.com/json/"

#define WIFI_CONNECTED_BIT   BIT0
#define WIFI_FAILED_BIT      BIT1
#define MAX_HTTP_RECV_BUFFER 2048

typedef struct {
    char city[64];
    char region[64];
    char country[64];
    double lat;
    double lon;
    bool valid;
} geo_location_t;

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_count = 0;
static char s_response_buffer[MAX_HTTP_RECV_BUFFER];
static size_t s_buffer_length = 0;
static bool s_response_too_large = false;
static geo_location_t s_current_location = { .valid = false };

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
        ESP_LOGI(TAG, "✅ 已拿到 IP，可以安全发起网络请求。");
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
        ESP_LOGI(TAG, "⏰ 网络时间已成功同步 (UTC+8)。");
    } else {
        ESP_LOGW(TAG, "网络授时超时；HTTPS 请求仍会继续。");
    }
}

static esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    if (event->event_id == HTTP_EVENT_ON_DATA && !s_response_too_large) {
        size_t free_space = sizeof(s_response_buffer) - s_buffer_length - 1;
        if ((size_t)event->data_len > free_space) {
            s_response_too_large = true;
            ESP_LOGE(TAG, "响应内容过大，超出缓冲区限制！");
            return ESP_FAIL;
        }

        memcpy(s_response_buffer + s_buffer_length, event->data, event->data_len);
        s_buffer_length += event->data_len;
        s_response_buffer[s_buffer_length] = '\0';
    }
    return ESP_OK;
}

static const char *weather_code_to_text(int weather_code)
{
    switch (weather_code) {
        case 0: return "晴朗 ☀️";
        case 1: case 2: case 3: return "多云 ⛅";
        case 45: case 48: return "有雾 🌫️";
        case 51: case 53: case 55: return "毛毛雨 🌦️";
        case 61: case 63: case 65: return "下雨 🌧️";
        case 71: case 73: case 75: return "下雪 ❄️";
        case 80: case 81: case 82: return "阵雨 🌧️";
        case 95: return "雷暴 ⛈️";
        default: return "其他天气";
    }
}

/**
 * 📍 阶段 1：解析 IP 定位返回的 JSON
 */
static bool parse_ip_geo_json(const char *json_text)
{
    cJSON *root = cJSON_Parse(json_text);
    if (root == NULL) {
        ESP_LOGE(TAG, "IP 定位 JSON 解析失败");
        return false;
    }

    cJSON *status = cJSON_GetObjectItemCaseSensitive(root, "status");
    if (!cJSON_IsString(status) || (status->valuestring == NULL) ||
        (strcmp(status->valuestring, "success") != 0)) {
        ESP_LOGE(TAG, "IP 定位接口状态异常");
        cJSON_Delete(root);
        return false;
    }

    cJSON *city = cJSON_GetObjectItemCaseSensitive(root, "city");
    cJSON *region = cJSON_GetObjectItemCaseSensitive(root, "regionName");
    cJSON *country = cJSON_GetObjectItemCaseSensitive(root, "country");
    cJSON *lat = cJSON_GetObjectItemCaseSensitive(root, "lat");
    cJSON *lon = cJSON_GetObjectItemCaseSensitive(root, "lon");

    if (!cJSON_IsNumber(lat) || !cJSON_IsNumber(lon)) {
        ESP_LOGE(TAG, "IP 定位经纬度字段缺失");
        cJSON_Delete(root);
        return false;
    }

    s_current_location.lat = lat->valuedouble;
    s_current_location.lon = lon->valuedouble;
    strncpy(s_current_location.city, cJSON_IsString(city) ? city->valuestring : "Unknown", sizeof(s_current_location.city) - 1);
    strncpy(s_current_location.region, cJSON_IsString(region) ? region->valuestring : "Unknown", sizeof(s_current_location.region) - 1);
    strncpy(s_current_location.country, cJSON_IsString(country) ? country->valuestring : "Unknown", sizeof(s_current_location.country) - 1);
    s_current_location.valid = true;

    ESP_LOGI(TAG, "--------------------------------------------------");
    ESP_LOGI(TAG, "📍 [IP 地理定位成功]");
    ESP_LOGI(TAG, "   • 所在国家: %s", s_current_location.country);
    ESP_LOGI(TAG, "   • 省份地区: %s", s_current_location.region);
    ESP_LOGI(TAG, "   • 当前城市: %s", s_current_location.city);
    ESP_LOGI(TAG, "   • 经纬坐标: 纬度 %.4f, 经度 %.4f", s_current_location.lat, s_current_location.lon);
    ESP_LOGI(TAG, "--------------------------------------------------");

    cJSON_Delete(root);
    return true;
}

static bool fetch_ip_location(void)
{
    memset(s_response_buffer, 0, sizeof(s_response_buffer));
    s_buffer_length = 0;
    s_response_too_large = false;

    esp_http_client_config_t config = {
        .url = IP_GEO_API_URL,
        .event_handler = http_event_handler,
        .timeout_ms = 8000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "无法创建 IP 定位客户端");
        return false;
    }

    ESP_LOGI(TAG, "🔍 正在通过当前 IP 查询物理地理位置...");
    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    bool success = false;

    if (err == ESP_OK && status_code == 200 && !s_response_too_large) {
        success = parse_ip_geo_json(s_response_buffer);
    } else {
        ESP_LOGE(TAG, "IP 定位请求失败：%s (HTTP %d)", esp_err_to_name(err), status_code);
    }

    esp_http_client_cleanup(client);
    return success;
}

/**
 * ⛅ 阶段 2：解析 HTTPS 接收到的天气 JSON
 */
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
        ESP_LOGE(TAG, "JSON 字段不完整或类型不对。");
        cJSON_Delete(root);
        return;
    }

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, " 🔒 [HTTPS 安全获取] %s - %s 实时天气报告: ", s_current_location.region, s_current_location.city);
    ESP_LOGI(TAG, "    - 天气状况: %s", weather_code_to_text(weather_code->valueint));
    ESP_LOGI(TAG, "    - 当前气温: %.1f ℃", temperature->valuedouble);
    ESP_LOGI(TAG, "    - 当前风速: %.1f km/h", wind_speed->valuedouble);
    ESP_LOGI(TAG, "==================================================");
    cJSON_Delete(root);
}

static void https_weather_task(void *arg)
{
    // 1. 先通过当前公网 IP 进行物理定位
    while (!s_current_location.valid) {
        if (!fetch_ip_location()) {
            ESP_LOGW(TAG, "IP 定位失败，5 秒后重试...");
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }

    // 2. 根据获取到的经纬度动态生成 HTTPS 天气请求 URL
    char weather_https_url[256];
    snprintf(weather_https_url, sizeof(weather_https_url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m%%2Cwind_speed_10m%%2Cweather_code",
             s_current_location.lat, s_current_location.lon);

    ESP_LOGI(TAG, "🔒 动态生成的 HTTPS 天气 URL: %s", weather_https_url);

    // 3. 循环轮询天气（每 60 秒一次）
    while (true) {
        memset(s_response_buffer, 0, sizeof(s_response_buffer));
        s_buffer_length = 0;
        s_response_too_large = false;

        // 🔒 配置 HTTPS 客户端并挂载全局 CA 根证书包
        esp_http_client_config_t config = {
            .url = weather_https_url,
            .event_handler = http_event_handler,
            .crt_bundle_attach = esp_crt_bundle_attach, // 👈 核心：启用 ESP-IDF 官方根证书包校验
            .timeout_ms = 10000,
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client == NULL) {
            ESP_LOGE(TAG, "无法创建 HTTPS 客户端。");
        } else {
            ESP_LOGI(TAG, "🌐 正在发起 TLS 握手与 HTTPS 天气请求...");
            esp_err_t err = esp_http_client_perform(client);
            int status_code = esp_http_client_get_status_code(client);

            if (s_response_too_large) {
                ESP_LOGW(TAG, "响应过大，本轮天气更新结束。");
            } else if (err != ESP_OK) {
                ESP_LOGE(TAG, "HTTPS 请求失败：%s", esp_err_to_name(err));
            } else if (status_code != 200) {
                ESP_LOGW(TAG, "服务器返回非 200 状态码：%d", status_code);
            } else {
                ESP_LOGI(TAG, "🔒 HTTPS 请求成功 (HTTP %d, 接收 %u 字节密文并解密)", 
                         status_code, (unsigned int)s_buffer_length);
                parse_weather_json(s_response_buffer);
            }
            esp_http_client_cleanup(client);
        }

        time_t now = 0;
        struct tm timeinfo = { 0 };
        time(&now);
        localtime_r(&now, &timeinfo);
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        ESP_LOGI(TAG, "🕒 当前时间：%s (下次刷新: 60秒后)", strftime_buf);

        vTaskDelay(pdMS_TO_TICKS(60 * 1000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, " 🚀 实验 4：IP 自动定位 + HTTPS 证书校验天气时钟 ");
    ESP_LOGI(TAG, "==================================================");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (!wifi_connect()) {
        ESP_LOGE(TAG, "Wi-Fi 连接失败，程序终止。");
        return;
    }

    sync_time();

    xTaskCreate(https_weather_task, "https_weather_task", 6 * 1024, NULL, 5, NULL);
}
