/*
 * ota.c — Backend-agnostic pull-based OTA for the ESP32 base template.
 *
 * The device fetches firmware from a configurable HTTPS URL.
 * Trigger sources:
 *   - MQTT message (handled externally — call ota_trigger())
 *   - Web UI button (handled via webserver.c → g_ota_trigger flag)
 *
 * Uses esp_https_ota.  Partition table must provide two app partitions
 * (ota_0 / ota_1) — see partitions.csv.
 *
 * More sophisticated flows (update notifications, subscription gating,
 * automatic server push) belong in the brand or product layer.
 */

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "nvs.h"
#include "ota.h"
#include "ota_policy.h"

static const char *TAG = "ota";

/* Same defaults/keys weld_processor.c already uses for ThingsBoard uploads --
 * ticket #8 reuses that config rather than introducing a second one. */
#define THINGSBOARD_HOST_DEFAULT "iot.mwe-inc.com"
#define TB_HOST_MAX_LEN  128
#define TB_TOKEN_MAX_LEN 128

/* Global trigger flag — also written by webserver.c */
volatile bool g_ota_trigger = false;

static ota_status_t s_status   = OTA_STATUS_IDLE;
static char s_firmware_url[512] = {0};

/* ── OTA task ─────────────────────────────────────────────────────────────── */

static void ota_task(void *pvParam)
{
    while (true) {
        /* Poll the trigger flag — 1 s interval is fine for this use case */
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (!g_ota_trigger) {
            continue;
        }
        g_ota_trigger = false;

        if (s_firmware_url[0] == '\0') {
            ESP_LOGW(TAG, "OTA triggered but no firmware URL configured");
            s_status = OTA_STATUS_FAILED;
            continue;
        }

        ESP_LOGI(TAG, "OTA starting — URL: %s", s_firmware_url);
        s_status = OTA_STATUS_DOWNLOADING;

        esp_http_client_config_t http_cfg = {
            .url             = s_firmware_url,
            .keep_alive_enable = true,
        };

        esp_https_ota_config_t ota_cfg = {
            .http_config = &http_cfg,
        };

        esp_err_t err = esp_https_ota(&ota_cfg);
        if (err == ESP_OK) {
            s_status = OTA_STATUS_SUCCESS;
            ESP_LOGI(TAG, "OTA success — rebooting in 3 s");
            vTaskDelay(pdMS_TO_TICKS(3000));
            esp_restart();
        } else {
            s_status = OTA_STATUS_FAILED;
            ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(err));
        }
    }
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void ota_init(const char *firmware_url)
{
    if (firmware_url && firmware_url[0] != '\0') {
        strncpy(s_firmware_url, firmware_url, sizeof(s_firmware_url) - 1);
    }

    xTaskCreate(ota_task, "ota", 8192, NULL, 5, NULL);
    ESP_LOGI(TAG, "OTA component initialised — firmware URL: %s",
             s_firmware_url[0] ? s_firmware_url : "(not set)");
}

void ota_trigger(void)
{
    g_ota_trigger = true;
}

ota_status_t ota_get_status(void)
{
    return s_status;
}

const char *ota_status_str(void)
{
    switch (s_status) {
    case OTA_STATUS_IDLE:        return "idle";
    case OTA_STATUS_DOWNLOADING: return "downloading";
    case OTA_STATUS_SUCCESS:     return "success";
    case OTA_STATUS_FAILED:      return "failed";
    default:                     return "unknown";
    }
}

/* Finds "key" in json, skips ": whitespace, reads the quoted value into val.
 * Same minimal ad-hoc approach webserver.c's json_str() uses -- this project
 * has no shared JSON parser by convention. */
static void json_str_field(const char *json, const char *key, char *val, size_t sz)
{
    val[0] = '\0';
    char needle[32];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p) {
        return;
    }
    p += strlen(needle);
    while (*p == ' ' || *p == '\t' || *p == ':') {
        p++;
    }
    if (*p != '"') {
        return;
    }
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < sz) {
        val[i++] = *p++;
    }
    val[i] = '\0';
}

bool ota_check_update(char *out_version, size_t out_size, bool *out_available)
{
    char tb_token[TB_TOKEN_MAX_LEN] = {0};
    char tb_host[TB_HOST_MAX_LEN] = {0};
    nvs_handle_t nvs;
    if (nvs_open("config", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(tb_token);
        nvs_get_str(nvs, "tb_token", tb_token, &len);
        len = sizeof(tb_host);
        nvs_get_str(nvs, "tb_url", tb_host, &len);
        nvs_close(nvs);
    }
    if (tb_host[0] == '\0') {
        strncpy(tb_host, THINGSBOARD_HOST_DEFAULT, sizeof(tb_host) - 1);
    }
    if (tb_token[0] == '\0') {
        ESP_LOGW(TAG, "OTA check: no server access token configured");
        return false;
    }

    char url[320];
    snprintf(url, sizeof(url), "https://%s/api/v1/%s/attributes?sharedKeys=fw_version",
             tb_host, tb_token);

    esp_http_client_config_t http_cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "OTA check: connect failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }
    esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);

    char resp[512] = {0};
    int total_read = 0;
    if (status >= 200 && status < 300) {
        int r;
        while (total_read < (int)sizeof(resp) - 1 &&
               (r = esp_http_client_read(client, resp + total_read,
                                          sizeof(resp) - 1 - total_read)) > 0) {
            total_read += r;
        }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (status < 200 || status >= 300 || total_read == 0) {
        ESP_LOGW(TAG, "OTA check: HTTP %d", status);
        return false;
    }

    char advertised[128] = {0};
    json_str_field(resp, "fw_version", advertised, sizeof(advertised));

    const esp_app_desc_t *app_desc = esp_app_get_description();
    bool available = ota_policy_update_available(app_desc->version, advertised);

    if (out_version) {
        strncpy(out_version, advertised, out_size - 1);
        out_version[out_size - 1] = '\0';
    }
    if (out_available) {
        *out_available = available;
    }

    ESP_LOGI(TAG, "OTA check: running=%s advertised=%s available=%d",
             app_desc->version, advertised, available);
    return true;
}
