#include "srv_mqtt.h"
#include "srv_event_bus.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SRV_MQTT";
static esp_mqtt_client_handle_t s_client = NULL;
static bool s_connected = false;

#define TOPIC_TELEMETRY  "esp32_journey/super_hub/telemetry"
#define TOPIC_COMMAND    "esp32_journey/super_hub/command"

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            s_connected = true;
            ESP_LOGI(TAG, "🟢 [MQTT 云端] 已连接至 Broker，订阅指令主题...");
            esp_mqtt_client_subscribe(s_client, TOPIC_COMMAND, 1);
            break;
        case MQTT_EVENT_DISCONNECTED:
            s_connected = false;
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "📥 收到 MQTT 下发指令: %.*s", event->data_len, event->data);
            srv_event_bus_post(HUB_EVENT_CMD_TOGGLE_LED, NULL, 0);
            break;
        default:
            break;
    }
}

esp_err_t srv_mqtt_start(const char *broker_uri)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
    };
    s_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    return esp_mqtt_client_start(s_client);
}

esp_err_t srv_mqtt_publish_telemetry(const char *json_data)
{
    if (!s_connected || !s_client) return ESP_ERR_INVALID_STATE;
    return (esp_mqtt_client_publish(s_client, TOPIC_TELEMETRY, json_data, 0, 1, 0) >= 0) ? ESP_OK : ESP_FAIL;
}

bool srv_mqtt_is_connected(void)
{
    return s_connected;
}
