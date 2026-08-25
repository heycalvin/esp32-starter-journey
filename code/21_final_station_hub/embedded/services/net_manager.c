#include "net_manager.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "sys_event_bus.h"
#include "bsp_led.h"

static const char *TAG = "NET_MGR";
static bool s_wifi_connected = false;
static bool s_mqtt_connected = false;
static char s_ip_addr[24] = "192.168.4.1";
static esp_mqtt_client_handle_t s_mqtt_client = NULL;

static esp_err_t net_manager_init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS 分区需要重建，正在擦除并重新初始化");
        ret = nvs_flash_erase();
        if (ret == ESP_OK) {
            ret = nvs_flash_init();
        }
    }
    return ret;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        sys_event_bus_post(HUB_EVT_WIFI_DISCONNECTED, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        esp_ip4addr_ntoa(&event->ip_info.ip, s_ip_addr, sizeof(s_ip_addr));
        s_wifi_connected = true;
        ESP_LOGI(TAG, "🌐 [Wi-Fi 联网成功] 获取到 IP 地址: %s", s_ip_addr);
        sys_event_bus_post(HUB_EVT_WIFI_CONNECTED, s_ip_addr, strlen(s_ip_addr) + 1);

        // 启动 SNTP
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "ntp.aliyun.com");
        esp_sntp_setservername(1, "time.apple.com");
        esp_sntp_init();
        setenv("TZ", "CST-8", 1);
        tzset();

        // 启动 MQTT
        if (s_mqtt_client) {
            esp_mqtt_client_start(s_mqtt_client);
        }
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    if (event_id == MQTT_EVENT_CONNECTED) {
        s_mqtt_connected = true;
        ESP_LOGI(TAG, "🟢 [MQTT 云端] 已连接到公共物联网 Broker (broker.emqx.io)！");
        esp_mqtt_client_subscribe(s_mqtt_client, "esp32/smarthub/command", 1);
    } else if (event_id == MQTT_EVENT_DISCONNECTED) {
        s_mqtt_connected = false;
        ESP_LOGW(TAG, "⚠️ [MQTT 云端] 连接断开，准备重连...");
    } else if (event_id == MQTT_EVENT_DATA) {
        char topic[64] = {0};
        char payload[128] = {0};
        int tlen = event->topic_len < 63 ? event->topic_len : 63;
        int plen = event->data_len < 127 ? event->data_len : 127;
        memcpy(topic, event->topic, tlen);
        memcpy(payload, event->data, plen);

        ESP_LOGI(TAG, "📨 [MQTT 收到指令] 主题: %s | 载荷: %s", topic, payload);
        if (strstr(payload, "TOGGLE_LED") || strstr(payload, "toggle")) {
            bsp_led_toggle();
        }
        sys_event_bus_post(HUB_EVT_MQTT_CMD_RECEIVED, payload, strlen(payload) + 1);
    }
}

esp_err_t net_manager_init(const char *ssid, const char *password)
{
    ESP_ERROR_CHECK(net_manager_init_nvs());
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {0};
    if (ssid && strlen(ssid) > 0) {
        strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
        strncpy((char *)wifi_config.sta.password, password ? password : "", sizeof(wifi_config.sta.password));
    } else {
        strncpy((char *)wifi_config.sta.ssid, "ESP32_Test", sizeof(wifi_config.sta.ssid));
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 配置 MQTT
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://broker.emqx.io:1883",
        .credentials.client_id = "esp32_smart_hub_device",
    };
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client) {
        esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    }

    ESP_LOGI(TAG, "🌐 [服务层] 无线网络中枢 (Wi-Fi STA + SNTP + MQTT) 初始化就绪！");
    return ESP_OK;
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

void net_manager_get_ip_str(char *buf, size_t max_len)
{
    strncpy(buf, s_ip_addr, max_len);
}

esp_err_t net_manager_publish_telemetry(const char *json_payload)
{
    if (!s_mqtt_connected || !s_mqtt_client) return ESP_ERR_INVALID_STATE;
    int msg_id = esp_mqtt_client_publish(s_mqtt_client, "esp32/smarthub/telemetry", json_payload, 0, 1, 0);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}
