#include "net_manager.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "mqtt_client.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "sys_event_bus.h"
#include "bsp_led.h"
#include "esp_http_client.h"
#include "cJSON.h"

static const char *TAG = "NET_MGR";
#define AP_SSID_NAME     "ESP32-Smart-Hub-WiFi"
#define NVS_WIFI_NS      "wifi_store"
#define NVS_KEY_SSID     "ssid"
#define NVS_KEY_PASS     "pass"

static net_mode_t s_net_mode = NET_MODE_PROVISIONING_AP;
static bool s_wifi_connected = false;
static bool s_mqtt_connected = false;
static char s_ip_addr[24] = "192.168.4.1";
static char s_cur_ssid[33] = AP_SSID_NAME;
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static TaskHandle_t s_dns_task_handle = NULL;
static int s_dns_socket = -1;

/* =========================================================================
 * 🌐 强制门户 DNS 劫持服务 (Captive Portal DNS Server)
 * ========================================================================= */
static void dns_server_task(void *pvParameters)
{
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    uint8_t rx_buffer[512];
    uint8_t tx_buffer[512];

    s_dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_dns_socket < 0) {
        ESP_LOGE(TAG, "❌ 创建 DNS Socket 失败");
        vTaskDelete(NULL);
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(53);

    if (bind(s_dns_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "❌ 绑定 DNS 端口 53 失败");
        close(s_dns_socket);
        s_dns_socket = -1;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "📡 [Captive Portal] DNS 劫持服务已在 53 端口就绪 (重定向至 192.168.4.1)");

    while (s_net_mode == NET_MODE_PROVISIONING_AP) {
        int len = recvfrom(s_dns_socket, rx_buffer, sizeof(rx_buffer), 0,
                           (struct sockaddr *)&client_addr, &client_addr_len);
        if (len < 12) continue;

        // 构造 DNS 响应报文，将任意域名解析为 192.168.4.1
        memcpy(tx_buffer, rx_buffer, len);
        tx_buffer[2] = 0x81; // QR=1, AA=1
        tx_buffer[3] = 0x80; // RA=1
        tx_buffer[6] = 0x00;
        tx_buffer[7] = 0x01; // Answer RRs = 1

        int idx = len;
        // Answer Section: Name Pointer (0xc00c), Type A (0x0001), Class IN (0x0001)
        tx_buffer[idx++] = 0xc0;
        tx_buffer[idx++] = 0x0c;
        tx_buffer[idx++] = 0x00;
        tx_buffer[idx++] = 0x01;
        tx_buffer[idx++] = 0x00;
        tx_buffer[idx++] = 0x01;
        // TTL = 60s
        tx_buffer[idx++] = 0x00;
        tx_buffer[idx++] = 0x00;
        tx_buffer[idx++] = 0x00;
        tx_buffer[idx++] = 0x3c;
        // Data Length = 4
        tx_buffer[idx++] = 0x00;
        tx_buffer[idx++] = 0x04;
        // IP: 192.168.4.1
        tx_buffer[idx++] = 192;
        tx_buffer[idx++] = 168;
        tx_buffer[idx++] = 4;
        tx_buffer[idx++] = 1;

        sendto(s_dns_socket, tx_buffer, idx, 0, (struct sockaddr *)&client_addr, client_addr_len);
    }

    if (s_dns_socket >= 0) {
        close(s_dns_socket);
        s_dns_socket = -1;
    }
    s_dns_task_handle = NULL;
    vTaskDelete(NULL);
}

/* =========================================================================
 * 💾 NVS Wi-Fi 凭证读写与出厂重置
 * ========================================================================= */
static bool load_wifi_credentials(char *ssid_buf, size_t ssid_len, char *pass_buf, size_t pass_len)
{
    nvs_handle_t h;
    if (nvs_open(NVS_WIFI_NS, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    size_t s_len = ssid_len;
    size_t p_len = pass_len;
    esp_err_t err1 = nvs_get_str(h, NVS_KEY_SSID, ssid_buf, &s_len);
    esp_err_t err2 = nvs_get_str(h, NVS_KEY_PASS, pass_buf, &p_len);
    nvs_close(h);
    return (err1 == ESP_OK && strlen(ssid_buf) > 0);
}

esp_err_t net_manager_save_credentials(const char *ssid, const char *password)
{
    if (!ssid || strlen(ssid) == 0) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_WIFI_NS, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        nvs_set_str(h, NVS_KEY_SSID, ssid);
        nvs_set_str(h, NVS_KEY_PASS, password ? password : "");
        nvs_commit(h);
        nvs_close(h);
        ESP_LOGI(TAG, "💾 [NVS 固化] Wi-Fi 凭据已成功写入 Flash: [%s]", ssid);
        return ESP_OK;
    }
    return err;
}

esp_err_t net_manager_reset_credentials(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_WIFI_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
        ESP_LOGW(TAG, "♻️ [NVS 擦除] Wi-Fi 凭据已重置，准备重启进入 AP 配网模式...");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
        return ESP_OK;
    }
    return ESP_FAIL;
}

/* =========================================================================
 * 🌍 动态公网 IP 地理位置解析
 * ========================================================================= */
static void fetch_ip_location_task(void *pvParameters)
{
    // 等待网络和 DNS 完全稳定
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "🌍 [IP 定位] 正在通过公网 IP 在线查询地理位置...");

    esp_http_client_config_t config = {
        .url = "http://ip-api.com/json/?lang=zh-CN",
        .timeout_ms = 6000,
        .method = HTTP_METHOD_GET,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "❌ 初始化 IP 定位 HTTP 客户端失败");
        vTaskDelete(NULL);
        return;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err == ESP_OK) {
        esp_http_client_fetch_headers(client);
        char *resp_buf = malloc(1024);
        if (resp_buf) {
            int read_len = esp_http_client_read(client, resp_buf, 1023);
            if (read_len > 0) {
                resp_buf[read_len] = '\0';
                ESP_LOGI(TAG, "🌍 [IP 定位响应] %s", resp_buf);

                cJSON *root = cJSON_Parse(resp_buf);
                if (root) {
                    cJSON *status = cJSON_GetObjectItem(root, "status");
                    if (status && strcmp(status->valuestring, "success") == 0) {
                        cJSON *city = cJSON_GetObjectItem(root, "city");
                        cJSON *district = cJSON_GetObjectItem(root, "district");
                        cJSON *region = cJSON_GetObjectItem(root, "regionName");

                        char final_loc[64] = {0};
                        const char *c_str = (city && city->valuestring) ? city->valuestring : "";
                        const char *d_str = (district && district->valuestring && strlen(district->valuestring) > 0) ? district->valuestring : "";
                        const char *r_str = (region && region->valuestring) ? region->valuestring : "";

                        if (strlen(c_str) > 0 && strlen(d_str) > 0) {
                            snprintf(final_loc, sizeof(final_loc), "%s · %s", c_str, d_str);
                        } else if (strlen(r_str) > 0 && strlen(c_str) > 0) {
                            snprintf(final_loc, sizeof(final_loc), "%s · %s", r_str, c_str);
                        } else if (strlen(c_str) > 0) {
                            snprintf(final_loc, sizeof(final_loc), "%s", c_str);
                        }

                        if (strlen(final_loc) > 0) {
                            ESP_LOGI(TAG, "📍 [动态 IP 定位成功] 识别当前位置为: %s", final_loc);
                            net_manager_set_location_str(final_loc);
                        }
                    }
                    cJSON_Delete(root);
                }
            }
            free(resp_buf);
        }
    } else {
        ESP_LOGW(TAG, "⚠️ IP 定位请求连接失败: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}

/* =========================================================================
 * 📶 Wi-Fi 与 IP 事件处理
 * ========================================================================= */
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        if (s_net_mode == NET_MODE_CONNECTED_STA) {
            ESP_LOGW(TAG, "⚠️ Wi-Fi 连接断开，正在尝试重连...");
            sys_event_bus_post(HUB_EVT_WIFI_DISCONNECTED, NULL, 0);
            vTaskDelay(pdMS_TO_TICKS(3000));
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        esp_ip4addr_ntoa(&event->ip_info.ip, s_ip_addr, sizeof(s_ip_addr));
        s_wifi_connected = true;
        s_net_mode = NET_MODE_CONNECTED_STA;
        ESP_LOGI(TAG, "🎉 [Wi-Fi 联网成功] 已连入家庭路由器！分配 IP: %s", s_ip_addr);
        sys_event_bus_post(HUB_EVT_WIFI_CONNECTED, s_ip_addr, strlen(s_ip_addr) + 1);

        // 启动 SNTP 网络授时
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "ntp.aliyun.com");
        esp_sntp_setservername(1, "cn.pool.ntp.org");
        esp_sntp_setservername(2, "time.apple.com");
        esp_sntp_init();
        setenv("TZ", "CST-8", 1);
        tzset();

        // 启动 MQTT
        if (s_mqtt_client) {
            esp_mqtt_client_start(s_mqtt_client);
        }

        // 启动动态公网 IP 地理位置解析任务
        xTaskCreate(fetch_ip_location_task, "ip_loc_task", 4096, NULL, 5, NULL);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    if (event_id == MQTT_EVENT_CONNECTED) {
        s_mqtt_connected = true;
        ESP_LOGI(TAG, "🟢 [MQTT 云端] 已连接到物联网 Broker (broker.emqx.io)！");
        esp_mqtt_client_subscribe(s_mqtt_client, "esp32/smarthub/command", 1);
    } else if (event_id == MQTT_EVENT_DISCONNECTED) {
        s_mqtt_connected = false;
        ESP_LOGW(TAG, "🔴 [MQTT 云端] 连接断开");
    }
}

esp_err_t net_manager_init(const char *fallback_ssid, const char *fallback_password)
{
    // 初始化时区
    setenv("TZ", "CST-8", 1);
    tzset();

    // 基准 RTC 时间保护
    time_t now;
    time(&now);
    if (now < 1704067200) {
        struct tm init_tm = {
            .tm_year = 2026 - 1900,
            .tm_mon  = 7,
            .tm_mday = 25,
            .tm_hour = 16,
            .tm_min  = 0,
            .tm_sec  = 0
        };
        struct timeval tv = { .tv_sec = mktime(&init_tm), .tv_usec = 0 };
        settimeofday(&tv, NULL);
    }

    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    char saved_ssid[33] = {0};
    char saved_pass[65] = {0};
    bool has_saved = load_wifi_credentials(saved_ssid, sizeof(saved_ssid), saved_pass, sizeof(saved_pass));

    if (has_saved) {
        // 模式 A：NVS 中存在已存 Wi-Fi，直接进入 STA 模式连接路由器
        s_net_mode = NET_MODE_CONNECTING_STA;
        strncpy(s_cur_ssid, saved_ssid, sizeof(s_cur_ssid) - 1);
        strncpy(s_ip_addr, "正在连接...", sizeof(s_ip_addr));

        wifi_config_t sta_config = {0};
        strncpy((char *)sta_config.sta.ssid, saved_ssid, sizeof(sta_config.sta.ssid));
        strncpy((char *)sta_config.sta.password, saved_pass, sizeof(sta_config.sta.password));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(TAG, "📡 [STA 模式] 读取到 NVS 已存 Wi-Fi: [%s]，正在连接路由器...", saved_ssid);
    } else {
        // 模式 B：首次使用或无 NVS 记录，开启 APSTA 混合模式与 Captive Portal 配网
        s_net_mode = NET_MODE_PROVISIONING_AP;
        strncpy(s_cur_ssid, AP_SSID_NAME, sizeof(s_cur_ssid) - 1);
        strncpy(s_ip_addr, "192.168.4.1", sizeof(s_ip_addr));

        wifi_config_t ap_config = {
            .ap = {
                .ssid = AP_SSID_NAME,
                .ssid_len = strlen(AP_SSID_NAME),
                .channel = 1,
                .max_connection = 4,
                .authmode = WIFI_AUTH_OPEN,
            }
        };

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(TAG, "🚀 [AP 配网模式] 发射热点: [%s], 请用手机连接并访问 http://192.168.4.1 进行配网", AP_SSID_NAME);

        // 启动 DNS 强制门户劫持任务
        xTaskCreate(dns_server_task, "dns_portal", 3072, NULL, 3, &s_dns_task_handle);
    }

    // 配置 MQTT 客户端
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://broker.emqx.io:1883",
        .credentials.client_id = "esp32_smart_hub_device",
    };
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client) {
        esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    }

    return ESP_OK;
}

net_mode_t net_manager_get_mode(void)
{
    return s_net_mode;
}

bool net_manager_is_provisioning(void)
{
    return (s_net_mode == NET_MODE_PROVISIONING_AP);
}

bool net_manager_is_wifi_connected(void)
{
    return s_wifi_connected;
}

bool net_manager_is_mqtt_connected(void)
{
    return s_mqtt_connected;
}

void net_manager_get_time_str(char *buf, size_t max_len)
{
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(buf, max_len, "%H:%M:%S", &timeinfo);
}

void net_manager_get_date_str(char *buf, size_t max_len)
{
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    const char *weekdays[] = {"日", "一", "二", "三", "四", "五", "六"};
    snprintf(buf, max_len, "%04d年%02d月%02d日 星期%s",
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday,
             weekdays[timeinfo.tm_wday % 7]);
}

void net_manager_get_uptime_str(char *buf, size_t max_len)
{
    int64_t uptime_sec = esp_timer_get_time() / 1000000;
    int hrs = (int)(uptime_sec / 3600);
    int mins = (int)((uptime_sec % 3600) / 60);
    int secs = (int)(uptime_sec % 60);
    snprintf(buf, max_len, "%02d:%02d:%02d (%llds)", hrs, mins, secs, uptime_sec);
}

static char s_cur_location[64] = "深圳市 · 南山区";
static char s_cur_weather[64]  = "晴朗 26°C · 空气优";

void net_manager_get_location_str(char *buf, size_t max_len)
{
    strncpy(buf, s_cur_location, max_len);
}

void net_manager_set_location_str(const char *loc_str)
{
    if (loc_str) {
        // 如果包含社区/附近等超长后缀，只截取城市与地区
        char temp[64];
        strncpy(temp, loc_str, sizeof(temp) - 1);
        temp[sizeof(temp) - 1] = '\0';
        char *p_dot = strstr(temp, " · ");
        if (p_dot) {
            // 检查第二个 · 并截断
            char *p_second = strstr(p_dot + 3, " · ");
            if (p_second) *p_second = '\0';
            // 截掉"附近"字样
            char *p_near = strstr(temp, "附近");
            if (p_near) *p_near = '\0';
            strncpy(s_cur_location, temp, sizeof(s_cur_location) - 1);
        } else {
            strncpy(s_cur_location, loc_str, sizeof(s_cur_location) - 1);
        }
    }
}

void net_manager_get_weather_str(char *buf, size_t max_len)
{
    strncpy(buf, s_cur_weather, max_len);
}

void net_manager_set_weather_str(const char *w_str)
{
    if (w_str) {
        strncpy(s_cur_weather, w_str, sizeof(s_cur_weather) - 1);
    }
}

void net_manager_get_ip_str(char *buf, size_t max_len)
{
    strncpy(buf, s_ip_addr, max_len);
}

void net_manager_get_ssid_str(char *buf, size_t max_len)
{
    strncpy(buf, s_cur_ssid, max_len);
}

esp_err_t net_manager_publish_telemetry(const char *json_payload)
{
    if (!s_mqtt_connected || !s_mqtt_client) return ESP_ERR_INVALID_STATE;
    int msg_id = esp_mqtt_client_publish(s_mqtt_client, "esp32/smarthub/telemetry", json_payload, 0, 1, 0);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}
