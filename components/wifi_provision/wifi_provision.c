/*
 * wifi_provision.c — WiFi station bring-up with SoftAP fallback.
 *
 * Station mode uses credentials from NVS ("config"/wifi_ssid,wifi_pass — the
 * keys webserver.c's POST /api/wifi handler writes), falling back to Kconfig
 * defaults (CONFIG_WIFI_SSID/CONFIG_WIFI_PASSWORD). If no SSID is available at
 * all, starts a pure SoftAP (CONFIG_WIFI_AP_*) forever — there's nothing to
 * retry until credentials are provisioned (which reboots the device anyway).
 *
 * If credentials exist but the device can't reach that network — either the
 * initial boot-time connect attempt fails/times out, or a previously-working
 * connection stays down for STEADY_STATE_FALLBACK_MS — a background monitor
 * task switches to WIFI_MODE_APSTA: the SoftAP comes up (so the device stays
 * reachable for monitoring/reconfiguration) while station reconnect attempts
 * keep retrying in the background every BACKGROUND_RETRY_INTERVAL_MS. Once
 * station reconnects, the monitor task drops back to pure STA mode.
 *
 * WiFi mode-switch calls (esp_wifi_set_mode et al.) only ever happen from the
 * monitor task, never from inside the event handler, to avoid reentrancy
 * issues with the WiFi driver's own event-processing task.
 */

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs.h"
#include "sdkconfig.h"

#include "wifi_provision.h"

static const char *TAG = "wifi_provision";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define STA_MAX_BOOT_RETRY 5
#define STA_CONNECT_TIMEOUT_MS 15000

/* How long a previously-working connection may stay down before the monitor
 * task brings up the SoftAP fallback (in addition to station retries, not
 * instead of). Long enough to ride out a router reboot without flapping into
 * AP mode; short enough that the device doesn't stay unreachable for long. */
#define STEADY_STATE_FALLBACK_MS (2 * 60 * 1000)

/* Retry cadence for station reconnect attempts while already in AP fallback
 * mode. Deliberately slower than the immediate retry used before fallback --
 * once in fallback, hammering esp_wifi_connect() serves no one. */
#define BACKGROUND_RETRY_INTERVAL_MS (30 * 1000)

#define MONITOR_TASK_STACK 4096
#define MONITOR_TASK_PRIORITY 3

static EventGroupHandle_t s_wifi_event_group;
static esp_event_handler_instance_t s_wifi_handler;
static esp_event_handler_instance_t s_ip_handler;
static int s_retry_count;
static bool s_boot_decided;

static esp_netif_t *s_ap_netif;
static bool s_have_credentials;

/* Written by the event handler, read by the monitor task. Simple scalar
 * writes (bool/int64_t), each field single-writer -- no mutex needed, same
 * pattern as weld_processor.c's write-activity tracking. */
static volatile bool s_sta_connected;
static volatile int64_t s_disconnected_since_ms; /* 0 = not currently mid-disconnect-streak */

/* Only touched by the monitor task -- tracks whether we're currently in the
 * AP+background-retry fallback state (WIFI_MODE_APSTA) vs. pure STA. */
static bool s_ap_fallback_active;

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
            return;
        }

        if (s_sta_connected) {
            s_disconnected_since_ms = esp_timer_get_time() / 1000;
        }
        s_sta_connected = false;

        /* Immediate retry while still in the "just dropped, hoping to
         * reconnect quickly" phase. Once already in AP fallback, the
         * monitor task's slower periodic cadence takes over instead --
         * otherwise every failed attempt would immediately trigger
         * another disconnect event, hammering esp_wifi_connect(). */
        if (!s_ap_fallback_active) {
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retry_count = 0;
        s_sta_connected = true;
        s_disconnected_since_ms = 0;
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

/* Attempts the initial boot-time station connect, bounded to
 * STA_CONNECT_TIMEOUT_MS. The event handler and STA netif stay registered
 * regardless of outcome -- on failure, the caller falls back to AP mode but
 * background reconnect attempts (driven by the monitor task) keep using the
 * same handler. */
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

    vEventGroupDelete(s_wifi_event_group);
    s_wifi_event_group = NULL;

    if (connected) {
        ESP_LOGI(TAG, "Connected to WiFi station: %s", ssid);
    } else {
        ESP_LOGW(TAG, "Station connect failed/timed out (retries=%d) at boot", s_retry_count);
    }

    return connected;
}

/* Shared AP config (SSID/password/channel/max_conn) applied by both the
 * pure-AP (no credentials) and AP+background-retry fallback paths. */
static void configure_ap(void)
{
    if (!s_ap_netif) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
    }

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

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    ESP_LOGI(TAG, "SoftAP up: SSID=%s — visit 192.168.4.1", CONFIG_WIFI_AP_SSID);
}

/* No credentials at all -- nothing to retry, pure AP forever until the
 * operator provisions credentials (which reboots the device). */
static void start_softap_pure(void)
{
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    configure_ap();
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* Have credentials, but station isn't currently working (boot failure or
 * steady-state outage past STEADY_STATE_FALLBACK_MS). Brings up the AP
 * alongside the still-configured STA interface so background reconnect
 * attempts (driven by the monitor task) keep trying without disrupting
 * anyone connected to the AP. */
static void start_softap_fallback(void)
{
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    configure_ap();
    ESP_LOGW(TAG, "Station unreachable — SoftAP fallback active alongside background retries");
}

static void wifi_monitor_task(void *arg)
{
    (void)arg;
    int64_t last_retry_ms = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        if (s_sta_connected) {
            if (s_ap_fallback_active) {
                ESP_LOGI(TAG, "Station reconnected — leaving SoftAP fallback");
                ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
                s_ap_fallback_active = false;
            }
            continue;
        }

        if (!s_ap_fallback_active) {
            int64_t disconnected_since = s_disconnected_since_ms;
            if (disconnected_since != 0) {
                int64_t down_ms = (esp_timer_get_time() / 1000) - disconnected_since;
                if (down_ms >= STEADY_STATE_FALLBACK_MS) {
                    ESP_LOGW(TAG, "Disconnected for %lld ms — falling back to SoftAP",
                             (long long)down_ms);
                    start_softap_fallback();
                    s_ap_fallback_active = true;
                    last_retry_ms = esp_timer_get_time() / 1000;
                }
            }
        } else {
            int64_t now_ms = esp_timer_get_time() / 1000;
            if (now_ms - last_retry_ms >= BACKGROUND_RETRY_INTERVAL_MS) {
                last_retry_ms = now_ms;
                esp_wifi_connect();
            }
        }
    }
}

bool wifi_provision_start(void)
{
    char ssid[33] = {0};
    char pass[65] = {0};
    load_credentials(ssid, sizeof(ssid), pass, sizeof(pass));
    s_have_credentials = (ssid[0] != '\0');

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    bool connected = false;
    if (s_have_credentials) {
        connected = try_connect_station(ssid, pass);
    } else {
        ESP_LOGI(TAG, "No WiFi credentials configured — starting SoftAP for setup");
    }

    if (!connected) {
        if (s_have_credentials) {
            /* Boot connect failed but we have somewhere to keep retrying --
             * go straight into fallback (AP + background retry), not pure AP. */
            s_disconnected_since_ms = esp_timer_get_time() / 1000;
            start_softap_fallback();
            s_ap_fallback_active = true;
        } else {
            start_softap_pure();
        }
    }

    if (s_have_credentials) {
        BaseType_t rc = xTaskCreate(wifi_monitor_task, "wifi_mon",
                                     MONITOR_TASK_STACK, NULL, MONITOR_TASK_PRIORITY, NULL);
        if (rc != pdPASS) {
            ESP_LOGE(TAG, "Failed to create WiFi monitor task");
        }
    }

    return connected;
}
