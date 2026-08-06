/*
 * wifi_provision.c — WiFi station bring-up with SoftAP fallback.
 *
 * Station mode uses credentials from NVS ("config"/wifi_ssid,wifi_pass — the
 * keys webserver.c's POST /api/wifi handler writes), falling back to Kconfig
 * defaults (CONFIG_WIFI_SSID/CONFIG_WIFI_PASSWORD). If no SSID is available,
 * or the initial boot-time connect attempt fails/times out, falls back to a
 * SoftAP (CONFIG_WIFI_AP_*) so credentials can be entered via the web UI.
 *
 * Once a station connection succeeds, the disconnect handler keeps retrying
 * indefinitely (steady-state reconnect) rather than falling back to SoftAP
 * mid-session — a dropped AP is expected to be recoverable without a reboot.
 */

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs.h"
#include "sdkconfig.h"

#include "wifi_provision.h"

static const char *TAG = "wifi_provision";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define STA_MAX_BOOT_RETRY 5
#define STA_CONNECT_TIMEOUT_MS 15000

static EventGroupHandle_t s_wifi_event_group;
static esp_event_handler_instance_t s_wifi_handler;
static esp_event_handler_instance_t s_ip_handler;
static int s_retry_count;
static bool s_boot_decided;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (!s_boot_decided) {
            if (s_retry_count < STA_MAX_BOOT_RETRY) {
                s_retry_count++;
                esp_wifi_connect();
            } else {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
        } else {
            /* Steady-state: keep retrying indefinitely so a dropped AP is
             * recovered without requiring a reboot. */
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retry_count = 0;
        if (!s_boot_decided) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

static void load_credentials(char *ssid, size_t ssid_size, char *pass, size_t pass_size)
{
    ssid[0] = '\0';
    pass[0] = '\0';

    nvs_handle_t nvs;
    if (nvs_open("config", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = ssid_size;
        nvs_get_str(nvs, "wifi_ssid", ssid, &len);
        len = pass_size;
        nvs_get_str(nvs, "wifi_pass", pass, &len);
        nvs_close(nvs);
    }

    if (ssid[0] == '\0') {
        strncpy(ssid, CONFIG_WIFI_SSID, ssid_size - 1);
        ssid[ssid_size - 1] = '\0';
        strncpy(pass, CONFIG_WIFI_PASSWORD, pass_size - 1);
        pass[pass_size - 1] = '\0';
    }
}

/* Attempts station connect. On success, leaves the reconnect handler
 * permanently registered. On failure, tears down STA event handling
 * entirely so the caller can fall back to SoftAP. */
static bool try_connect_station(const char *ssid, const char *pass)
{
    esp_netif_create_default_wifi_sta();

    s_wifi_event_group = xEventGroupCreate();
    s_retry_count = 0;
    s_boot_decided = false;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &s_wifi_handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &s_ip_handler));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                            pdFALSE, pdFALSE,
                                            pdMS_TO_TICKS(STA_CONNECT_TIMEOUT_MS));
    s_boot_decided = true;

    bool connected = (bits & WIFI_CONNECTED_BIT) != 0;

    if (connected) {
        ESP_LOGI(TAG, "Connected to WiFi station: %s", ssid);
    } else {
        ESP_LOGW(TAG, "Station connect failed/timed out (retries=%d) — falling back to SoftAP",
                 s_retry_count);
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_handler);
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_ip_handler);
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
        ESP_ERROR_CHECK(esp_wifi_stop());
    }

    return connected;
}

static void start_softap(void)
{
    esp_netif_create_default_wifi_ap();

    wifi_config_t ap_config = {
        .ap = {
            .channel = CONFIG_WIFI_AP_CHANNEL,
            .max_connection = CONFIG_WIFI_AP_MAX_CONN,
            .authmode = (strlen(CONFIG_WIFI_AP_PASSWORD) == 0) ? WIFI_AUTH_OPEN
                                                                 : WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char *)ap_config.ap.ssid, CONFIG_WIFI_AP_SSID, sizeof(ap_config.ap.ssid) - 1);
    ap_config.ap.ssid_len = strlen(CONFIG_WIFI_AP_SSID);
    strncpy((char *)ap_config.ap.password, CONFIG_WIFI_AP_PASSWORD,
            sizeof(ap_config.ap.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP started: SSID=%s — connect and visit 192.168.4.1 to configure WiFi",
             CONFIG_WIFI_AP_SSID);
}

bool wifi_provision_start(void)
{
    char ssid[33] = {0};
    char pass[65] = {0};
    load_credentials(ssid, sizeof(ssid), pass, sizeof(pass));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    bool connected = false;
    if (ssid[0] != '\0') {
        connected = try_connect_station(ssid, pass);
    } else {
        ESP_LOGI(TAG, "No WiFi credentials configured — starting SoftAP for setup");
    }

    if (!connected) {
        start_softap();
    }

    return connected;
}
