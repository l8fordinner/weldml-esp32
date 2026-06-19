/*
 * ESP32 Base Template — main.c
 *
 * WiFi behaviour:
 *   - Credentials in NVS → STA mode, connect to configured network.
 *     MQTT + OTA start once connected.
 *   - No credentials → SoftAP mode ("ESP32-Setup" by default).
 *     Connect phone/laptop to that network, browse to 192.168.4.1,
 *     enter WiFi credentials, device saves them to NVS and reboots.
 *
 * No product-specific code, backend integrations, or branding lives here.
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "webserver.h"
#include "mqtt.h"   /* from components/app_mqtt */
#include "ota.h"

static const char *TAG = "main";

/* WiFi STA event group bits */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_count = 0;
#define WIFI_MAX_RETRY 5

/* ── NVS helpers ─────────────────────────────────────────────────────────── */

static void nvs_get_str_or_default(nvs_handle_t nvs, const char *key,
                                   char *buf, size_t buf_len,
                                   const char *fallback)
{
    size_t len = buf_len;
    esp_err_t err = nvs_get_str(nvs, key, buf, &len);
    if (err != ESP_OK || buf[0] == '\0') {
        strncpy(buf, fallback, buf_len - 1);
        buf[buf_len - 1] = '\0';
    }
}

/* ── WiFi STA ────────────────────────────────────────────────────────────── */

static void sta_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_count++;
            ESP_LOGW(TAG, "WiFi disconnected — retry %d/%d",
                     s_retry_count, WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "WiFi connection failed after %d retries",
                     WIFI_MAX_RETRY);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi connected — IP: " IPSTR,
                 IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static bool wifi_init_sta(const char *ssid, const char *password)
{
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &sta_event_handler, NULL,
        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &sta_event_handler, NULL,
        &instance_got_ip));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid,
            sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password,
            sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(15000));

    if (bits & WIFI_CONNECTED_BIT) {
        return true;
    }

    ESP_LOGW(TAG, "STA connection failed — staying online via webserver");
    return false;
}

/* ── WiFi SoftAP ─────────────────────────────────────────────────────────── */

static void ap_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *e =
            (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "Setup AP: client connected (AID %d)", e->aid);
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *e =
            (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "Setup AP: client disconnected (AID %d)", e->aid);
    }
}

static void wifi_init_softap(void)
{
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &ap_event_handler, NULL, NULL));

    wifi_config_t ap_config = {
        .ap = {
            .channel         = CONFIG_WIFI_AP_CHANNEL,
            .max_connection  = CONFIG_WIFI_AP_MAX_CONN,
            .authmode        = WIFI_AUTH_OPEN,
        },
    };

    strncpy((char *)ap_config.ap.ssid, CONFIG_WIFI_AP_SSID,
            sizeof(ap_config.ap.ssid) - 1);
    ap_config.ap.ssid_len = (uint8_t)strlen(CONFIG_WIFI_AP_SSID);

    if (strlen(CONFIG_WIFI_AP_PASSWORD) > 0) {
        strncpy((char *)ap_config.ap.password, CONFIG_WIFI_AP_PASSWORD,
                sizeof(ap_config.ap.password) - 1);
        ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Setup AP started — SSID: \"%s\"  IP: 192.168.4.1",
             CONFIG_WIFI_AP_SSID);
    ESP_LOGI(TAG, "Connect your phone/laptop to \"%s\" then browse to http://192.168.4.1",
             CONFIG_WIFI_AP_SSID);
}

/* ── app_main ────────────────────────────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 Base Template starting — IDF v%s",
             esp_get_idf_version());

    /* NVS init */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Shared network init */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Read config from NVS (fall back to Kconfig defaults) */
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open("config", NVS_READWRITE, &nvs));

    char ssid[64]      = CONFIG_WIFI_SSID;
    char password[64]  = CONFIG_WIFI_PASSWORD;
    char mqtt_url[256] = CONFIG_MQTT_BROKER_URL;
    char ota_url[512]  = CONFIG_OTA_FIRMWARE_URL;

    nvs_get_str_or_default(nvs, "wifi_ssid", ssid,     sizeof(ssid),     CONFIG_WIFI_SSID);
    nvs_get_str_or_default(nvs, "wifi_pass", password, sizeof(password), CONFIG_WIFI_PASSWORD);
    nvs_get_str_or_default(nvs, "mqtt_url",  mqtt_url, sizeof(mqtt_url), CONFIG_MQTT_BROKER_URL);
    nvs_get_str_or_default(nvs, "ota_url",   ota_url,  sizeof(ota_url),  CONFIG_OTA_FIRMWARE_URL);

    nvs_close(nvs);

    /* WiFi — STA if credentials exist, SoftAP otherwise */
    bool wifi_ok = false;
    if (ssid[0] != '\0') {
        wifi_ok = wifi_init_sta(ssid, password);
    } else {
        wifi_init_softap();
    }

    /* Webserver — always starts (serves setup UI on AP, config UI on STA) */
    webserver_start();

    /* MQTT + OTA only useful once we have upstream network access */
    if (wifi_ok) {
        mqtt_start(mqtt_url);
        ota_init(ota_url);
    }

    ESP_LOGI(TAG, "Startup complete");
}
