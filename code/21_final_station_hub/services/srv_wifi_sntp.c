#include "srv_wifi_sntp.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include <time.h>
#include <string.h>

static const char *TAG = "SRV_WIFI_SNTP";
static bool s_connected = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "🎉 [Wi-Fi 联网成功] IP 地址: " IPSTR, IP2STR(&event->ip_info.ip));
        s_connected = true;

        // 启动 SNTP
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "ntp.aliyun.com");
        esp_sntp_init();
        setenv("TZ", "CST-8", 1);
        tzset();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        esp_wifi_connect();
    }
}

esp_err_t srv_wifi_sntp_start(const char *ssid, const char *password)
{
    esp_netif_init();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = { 0 };
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    return esp_wifi_start();
}

bool srv_wifi_is_connected(void)
{
    return s_connected;
}

void srv_wifi_get_time_str(char *buf, size_t max_len)
{
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(buf, max_len, "%Y-%m-%d %H:%M:%S", &timeinfo);
}
