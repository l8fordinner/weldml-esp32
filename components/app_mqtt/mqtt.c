/*
 * mqtt.c — Generic MQTT client wrapper for the ESP32 base template.
 *
 * Wraps esp-mqtt with:
 *   - Automatic reconnection on disconnect
 *   - Status logging
 *   - Simple publish/subscribe API
 *
 * No topic structure, backend conventions, or business logic lives here.
 * Those belong in the brand-template or product forks.
 */

#include <string.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "mqtt.h"

static const char *TAG = "mqtt";
static esp_mqtt_client_handle_t s_client = NULL;
static bool s_connected = false;

/* ── Event handler ───────────────────────────────────────────────────────── */

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "Connected to broker");
        break;

    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "Disconnected from broker — will reconnect automatically");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "Subscribed — msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "Unsubscribed — msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "Published — msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        /* Brand/product forks: register additional event handlers here
           via esp_mqtt_client_register_event() to process incoming messages
           without modifying this component. */
        ESP_LOGD(TAG, "Received [%.*s]: %.*s",
                 event->topic_len, event->topic,
                 event->data_len, event->data);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error: type=%d",
                 event->error_handle->error_type);
        break;

    default:
        break;
    }
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void mqtt_start(const char *broker_url)
{
    if (s_client) {
        ESP_LOGW(TAG, "mqtt_start called while already running");
        return;
    }

    esp_mqtt_client_config_t config = {
        .broker.address.uri = broker_url,
    };

    s_client = esp_mqtt_client_init(&config);
    if (!s_client) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
    ESP_LOGI(TAG, "MQTT client started — broker: %s", broker_url);
}

void mqtt_stop(void)
{
    if (s_client) {
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
        s_connected = false;
    }
}

int mqtt_publish(const char *topic, const char *payload, int len,
                 int qos, bool retain)
{
    if (!s_client || !s_connected) {
        ESP_LOGW(TAG, "mqtt_publish: not connected");
        return -1;
    }
    return esp_mqtt_client_publish(s_client, topic, payload, len, qos,
                                   (int)retain);
}

int mqtt_subscribe(const char *topic, int qos)
{
    if (!s_client || !s_connected) {
        ESP_LOGW(TAG, "mqtt_subscribe: not connected");
        return -1;
    }
    return esp_mqtt_client_subscribe(s_client, topic, qos);
}

bool mqtt_is_connected(void)
{
    return s_connected;
}
