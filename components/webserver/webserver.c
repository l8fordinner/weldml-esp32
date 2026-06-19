/*
 * webserver.c — Generic HTTP server for the ESP32 base template.
 *
 * Serves static files from SPIFFS (/web partition) and provides REST
 * endpoints for reading/writing device configuration stored in NVS.
 *
 * Endpoints
 * ---------
 *   GET  /              → index.html  (WiFi setup)
 *   GET  /config        → config.html (MQTT broker config)
 *   GET  /ota           → ota.html    (OTA trigger + status)
 *   GET  /status        → status.html (device info)
 *   GET  /api/status    → JSON device status
 *   POST /api/wifi      → JSON {ssid, password} → save to NVS, reconnect
 *   POST /api/config    → JSON {mqtt_url, mqtt_port} → save to NVS
 *   POST /api/ota       → trigger OTA update
 */

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "webserver.h"

static const char *TAG = "webserver";
static httpd_handle_t s_server = NULL;

/* ── SPIFFS ──────────────────────────────────────────────────────────────── */

static void spiffs_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path              = "/web",
        .partition_label        = "spiffs",
        .max_files              = 8,
        .format_if_mount_failed = true,
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
    } else {
        size_t total = 0, used = 0;
        esp_spiffs_info("spiffs", &total, &used);
        ESP_LOGI(TAG, "SPIFFS mounted: %d/%d bytes used", (int)used, (int)total);
    }
}

/* ── Static file helper ──────────────────────────────────────────────────── */

static esp_err_t serve_file(httpd_req_t *req, const char *path,
                             const char *content_type)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGW(TAG, "File not found: %s", path);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, content_type);
    char buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        httpd_resp_send_chunk(req, buf, (ssize_t)n);
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* ── Page handlers ───────────────────────────────────────────────────────── */

static esp_err_t handler_index(httpd_req_t *req)
{
    return serve_file(req, "/web/index.html", "text/html");
}

static esp_err_t handler_config(httpd_req_t *req)
{
    return serve_file(req, "/web/config.html", "text/html");
}

static esp_err_t handler_ota(httpd_req_t *req)
{
    return serve_file(req, "/web/ota.html", "text/html");
}

static esp_err_t handler_status(httpd_req_t *req)
{
    return serve_file(req, "/web/status.html", "text/html");
}

static esp_err_t handler_style(httpd_req_t *req)
{
    return serve_file(req, "/web/style.css", "text/css");
}

static esp_err_t handler_appjs(httpd_req_t *req)
{
    return serve_file(req, "/web/app.js", "application/javascript");
}

/* ── API: GET /api/status ────────────────────────────────────────────────── */

static esp_err_t handler_api_status(httpd_req_t *req)
{
    wifi_ap_record_t ap_info = {0};
    esp_wifi_sta_get_ap_info(&ap_info);

    char json[512];
    snprintf(json, sizeof(json),
             "{\"idf_version\":\"%s\","
             "\"free_heap\":%lu,"
             "\"uptime_ms\":%llu,"
             "\"wifi_rssi\":%d,"
             "\"wifi_ssid\":\"%s\"}",
             esp_get_idf_version(),
             (unsigned long)esp_get_free_heap_size(),
             (unsigned long long)(esp_timer_get_time() / 1000),
             ap_info.rssi,
             (char *)ap_info.ssid);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}

/* ── JSON string extractor ───────────────────────────────────────────────── */

/* Finds "key" in json, skips ': whitespace, reads quoted value into val.
   Handles both {"key":"val"} and {"key": "val"} (Python-style spacing). */
static void json_str(const char *json, const char *key, char *val, size_t sz)
{
    char needle[80];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p) return;
    p += strlen(needle);
    while (*p == ' ' || *p == '\t' || *p == ':') p++;
    if (*p != '"') return;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < sz) val[i++] = *p++;
    val[i] = '\0';
}

/* ── API: POST /api/wifi ─────────────────────────────────────────────────── */

static esp_err_t handler_api_wifi(httpd_req_t *req)
{
    char body[256] = {0};
    int ret = httpd_req_recv(req, body, sizeof(body) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }

    char ssid[64] = {0}, pass[64] = {0};
    json_str(body, "ssid",     ssid, sizeof(ssid));
    json_str(body, "password", pass, sizeof(pass));

    if (ssid[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ssid required");
        return ESP_FAIL;
    }

    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open("config", NVS_READWRITE, &nvs));
    nvs_set_str(nvs, "wifi_ssid", ssid);
    nvs_set_str(nvs, "wifi_pass", pass);
    nvs_commit(nvs);
    nvs_close(nvs);

    ESP_LOGI(TAG, "WiFi credentials saved — SSID: %s", ssid);
    httpd_resp_sendstr(req, "{\"ok\":true,\"message\":\"Saved. Rebooting.\"}");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

/* ── API: POST /api/config ───────────────────────────────────────────────── */

static esp_err_t handler_api_config(httpd_req_t *req)
{
    char body[512] = {0};
    int ret = httpd_req_recv(req, body, sizeof(body) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }

    char mqtt_url[256] = {0};
    char ota_url[512]  = {0};
    json_str(body, "mqtt_url", mqtt_url, sizeof(mqtt_url));
    json_str(body, "ota_url",  ota_url,  sizeof(ota_url));

    if (mqtt_url[0] != '\0' || ota_url[0] != '\0') {
        nvs_handle_t nvs;
        ESP_ERROR_CHECK(nvs_open("config", NVS_READWRITE, &nvs));
        if (mqtt_url[0] != '\0') {
            nvs_set_str(nvs, "mqtt_url", mqtt_url);
            ESP_LOGI(TAG, "MQTT URL saved: %s", mqtt_url);
        }
        if (ota_url[0] != '\0') {
            nvs_set_str(nvs, "ota_url", ota_url);
            ESP_LOGI(TAG, "OTA URL saved: %s", ota_url);
        }
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/* ── API: POST /api/factory-reset ───────────────────────────────────────── */

static esp_err_t handler_api_factory_reset(httpd_req_t *req)
{
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open("config", NVS_READWRITE, &nvs));
    nvs_erase_all(nvs);
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG, "Factory reset — NVS erased, rebooting");
    httpd_resp_sendstr(req, "{\"ok\":true,\"message\":\"NVS erased. Rebooting.\"}");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

/* ── API: POST /api/ota ──────────────────────────────────────────────────── */

/* OTA trigger — the actual update runs in ota.c; we just set a flag here
   via a simple global that ota.c polls.  A cleaner approach (event or queue)
   belongs in a product fork once the architecture is more settled. */
extern volatile bool g_ota_trigger;

static esp_err_t handler_api_ota(httpd_req_t *req)
{
    g_ota_trigger = true;
    ESP_LOGI(TAG, "OTA triggered via web UI");
    httpd_resp_sendstr(req, "{\"ok\":true,\"message\":\"OTA started\"}");
    return ESP_OK;
}

/* ── URI table ───────────────────────────────────────────────────────────── */

static const httpd_uri_t s_uris[] = {
    { .uri = "/",            .method = HTTP_GET,  .handler = handler_index   },
    { .uri = "/config",      .method = HTTP_GET,  .handler = handler_config  },
    { .uri = "/ota",         .method = HTTP_GET,  .handler = handler_ota     },
    { .uri = "/status",      .method = HTTP_GET,  .handler = handler_status  },
    { .uri = "/style.css",   .method = HTTP_GET,  .handler = handler_style   },
    { .uri = "/app.js",      .method = HTTP_GET,  .handler = handler_appjs   },
    { .uri = "/api/status",  .method = HTTP_GET,  .handler = handler_api_status },
    { .uri = "/api/wifi",          .method = HTTP_POST, .handler = handler_api_wifi          },
    { .uri = "/api/factory-reset", .method = HTTP_POST, .handler = handler_api_factory_reset },
    { .uri = "/api/config",  .method = HTTP_POST, .handler = handler_api_config },
    { .uri = "/api/ota",     .method = HTTP_POST, .handler = handler_api_ota    },
};

/* ── Public API ──────────────────────────────────────────────────────────── */

void webserver_start(void)
{
    spiffs_init();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 13;

    if (httpd_start(&s_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    for (int i = 0; i < (int)(sizeof(s_uris) / sizeof(s_uris[0])); i++) {
        httpd_register_uri_handler(s_server, &s_uris[i]);
    }

    ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
}

void webserver_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    esp_vfs_spiffs_unregister("spiffs");
}
