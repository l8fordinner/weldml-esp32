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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "ota.h"

static const char *TAG = "ota";

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
