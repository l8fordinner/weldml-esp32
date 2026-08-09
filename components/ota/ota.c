/*
 * ota.c — ThingsBoard-backed OTA for WeldML (Milestone 3, tickets #8/#10).
 *
 * The device fetches firmware from ThingsBoard's own OTA package feature,
 * over the same HTTPS connection/access-token weld-result uploads already
 * use (docs/OPEN_QUESTIONS.md Q17). Trigger source: web UI button only
 * (handled via webserver.c → g_ota_trigger flag) — no automatic/unattended
 * update.
 *
 * Partition table must provide two app partitions (ota_0 / ota_1) — see
 * the board-specific partitions.csv under boards/.
 */

#include <stdbool.h>
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
#include "mbedtls/sha256.h"
#include "nvs.h"
#include "ota.h"
#include "ota_policy.h"

static const char *TAG = "ota";

/* Same defaults/keys weld_processor.c already uses for ThingsBoard uploads --
 * ticket #8 reuses that config rather than introducing a second one. */
#define THINGSBOARD_HOST_DEFAULT "iot.mwe-inc.com"
#define TB_HOST_MAX_LEN  128
#define TB_TOKEN_MAX_LEN 128
#define FW_TITLE_MAX_LEN 64
#define FW_VERSION_MAX_LEN 64
#define FW_CHECKSUM_MAX_LEN 128
#define FW_ALGORITHM_MAX_LEN 16
#define ATTR_URL_MAX_LEN (TB_HOST_MAX_LEN + TB_TOKEN_MAX_LEN + 96)

/* Matches ESP-IDF's own esp_https_ota default chunk size convention. Heap,
 * not stack -- ota_task's stack is shared with mbedtls's TLS handshake
 * working set during the same call. */
#define DOWNLOAD_BUF_SIZE 4096

/* Global trigger flag — also written by webserver.c */
volatile bool g_ota_trigger = false;

/* Global MSC-write-in-progress flag — written by weld_processor.c, see ota.h. */
volatile bool g_weld_write_active = false;

static ota_status_t s_status = OTA_STATUS_IDLE;
static char s_status_reason[128] = {0};

static void set_failed(const char *reason)
{
    s_status = OTA_STATUS_FAILED;
    snprintf(s_status_reason, sizeof(s_status_reason), "%s", reason);
    ESP_LOGE(TAG, "OTA failed: %s", reason);
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

/* Percent-encodes into out (out_size includes the NUL), keeping RFC 3986
 * unreserved characters literal. Used for fw_title/fw_version, whose exact
 * character set ThingsBoard doesn't otherwise constrain -- a package titled
 * with a space or "&" must not corrupt the firmware-download query string. */
static void url_encode(const char *in, char *out, size_t out_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0' && j + 1 < out_size; i++) {
        unsigned char c = (unsigned char)in[i];
        bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                           (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                           c == '.' || c == '~';
        if (unreserved) {
            out[j++] = (char)c;
        } else if (j + 3 < out_size) {
            out[j++] = '%';
            out[j++] = hex[c >> 4];
            out[j++] = hex[c & 0xF];
        } else {
            break;
        }
    }
    out[j] = '\0';
}

/* Reads tb_token/tb_url from NVS (falling back to THINGSBOARD_HOST_DEFAULT
 * for the host). Returns false if no token is configured. Shared by
 * ota_check_update() and run_update() -- both need it before making any
 * ThingsBoard request. */
static bool nvs_read_tb_config(char *tb_host, size_t host_size,
                                char *tb_token, size_t token_size)
{
    tb_host[0] = '\0';
    tb_token[0] = '\0';
    nvs_handle_t nvs;
    if (nvs_open("config", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = token_size;
        nvs_get_str(nvs, "tb_token", tb_token, &len);
        len = host_size;
        nvs_get_str(nvs, "tb_url", tb_host, &len);
        nvs_close(nvs);
    }
    if (tb_host[0] == '\0') {
        strncpy(tb_host, THINGSBOARD_HOST_DEFAULT, host_size - 1);
    }
    return tb_token[0] != '\0';
}

/* Fetches shared attributes (comma-separated `keys_csv`) from ThingsBoard
 * into resp (NUL-terminated flat JSON object). Shared by ota_check_update()
 * and run_update() -- keeping the NVS-read + HTTP GET + response-buffering
 * logic in one place avoids the two call sites drifting apart. Returns
 * false on any connection/HTTP-status/empty-body failure. */
static bool fetch_shared_attributes(const char *tb_host, const char *tb_token,
                                     const char *keys_csv,
                                     char *resp, size_t resp_size)
{
    char url[ATTR_URL_MAX_LEN];
    snprintf(url, sizeof(url), "https://%s/api/v1/%s/attributes?sharedKeys=%s",
             tb_host, tb_token, keys_csv);

    esp_http_client_config_t http_cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return false;
    }
    esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);

    resp[0] = '\0';
    int total_read = 0;
    if (status >= 200 && status < 300) {
        int r;
        while (total_read < (int)resp_size - 1 &&
               (r = esp_http_client_read(client, resp + total_read,
                                          resp_size - 1 - total_read)) > 0) {
            total_read += r;
        }
        resp[total_read] = '\0';
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    return (status >= 200 && status < 300 && total_read > 0);
}

/* ── OTA task ─────────────────────────────────────────────────────────────── */

/*
 * Ticket #10: real ThingsBoard-backed download + checksum-gated flash.
 * Fetches fw_title/fw_version/fw_checksum/fw_checksum_algorithm fresh (not
 * cached from an earlier ota_check_update() call -- avoids acting on stale
 * attributes between the user viewing "Update available" and clicking the
 * button), downloads the image into the inactive OTA partition while
 * accumulating a SHA-256 hash of the exact bytes written, and only calls
 * esp_ota_set_boot_partition() -- the one call that actually makes the new
 * image bootable -- if that hash matches ThingsBoard's advertised checksum
 * AND ESP-IDF's own esp_ota_end() image-structure validation both pass. Any
 * failure path calls esp_ota_abort() instead, leaving the previous boot
 * partition untouched. See docs/OPEN_QUESTIONS.md Q17/Q19.
 */
static void run_update(void)
{
    /* Clear any reason left over from a previous failed attempt -- otherwise
     * a fail-then-succeed sequence would report ota_status:"success" next to
     * a stale ota_reason, contradicting ota_status_reason()'s documented
     * contract. */
    s_status_reason[0] = '\0';

    /* Q3/Q6/Q12 SD-ownership + CLAUDE.md flash-safety: never write to flash
     * while a robot is actively streaming a file to the SD card over
     * USB-MSC ("Flash writes require the USB bus to be quiescent"). Ticket
     * #9's webserver_stop() bracket already makes this endpoint unreachable
     * during the processing phase, but not during the earlier WRITING phase
     * -- check explicitly rather than relying on that side effect. */
    if (g_weld_write_active) {
        set_failed("device busy — weld write in progress, try again shortly");
        return;
    }

    char tb_token[TB_TOKEN_MAX_LEN];
    char tb_host[TB_HOST_MAX_LEN];
    if (!nvs_read_tb_config(tb_host, sizeof(tb_host), tb_token, sizeof(tb_token))) {
        set_failed("no server access token configured");
        return;
    }

    char attr_resp[512];
    if (!fetch_shared_attributes(tb_host, tb_token,
                                  "fw_title,fw_version,fw_checksum,fw_checksum_algorithm",
                                  attr_resp, sizeof(attr_resp))) {
        set_failed("could not fetch firmware attributes");
        return;
    }

    char fw_title[FW_TITLE_MAX_LEN] = {0};
    char fw_version[FW_VERSION_MAX_LEN] = {0};
    char fw_checksum[FW_CHECKSUM_MAX_LEN] = {0};
    char fw_algorithm[FW_ALGORITHM_MAX_LEN] = {0};
    json_str_field(attr_resp, "fw_title", fw_title, sizeof(fw_title));
    json_str_field(attr_resp, "fw_version", fw_version, sizeof(fw_version));
    json_str_field(attr_resp, "fw_checksum", fw_checksum, sizeof(fw_checksum));
    json_str_field(attr_resp, "fw_checksum_algorithm", fw_algorithm, sizeof(fw_algorithm));

    if (fw_title[0] == '\0' || fw_version[0] == '\0') {
        set_failed("no firmware package configured");
        return;
    }

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        set_failed("no inactive OTA partition available");
        return;
    }

    char fw_title_enc[FW_TITLE_MAX_LEN * 3];
    char fw_version_enc[FW_VERSION_MAX_LEN * 3];
    url_encode(fw_title, fw_title_enc, sizeof(fw_title_enc));
    url_encode(fw_version, fw_version_enc, sizeof(fw_version_enc));

    char fw_url[TB_HOST_MAX_LEN + TB_TOKEN_MAX_LEN + sizeof(fw_title_enc) + sizeof(fw_version_enc) + 48];
    snprintf(fw_url, sizeof(fw_url), "https://%s/api/v1/%s/firmware?title=%s&version=%s",
             tb_host, tb_token, fw_title_enc, fw_version_enc);

    ESP_LOGI(TAG, "OTA starting — title=%s version=%s partition=%s",
             fw_title, fw_version, update_partition->label);
    s_status = OTA_STATUS_DOWNLOADING;

    esp_http_client_config_t fw_cfg = {
        .url = fw_url,
        .method = HTTP_METHOD_GET,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30000,
    };
    esp_http_client_handle_t fw_client = esp_http_client_init(&fw_cfg);
    esp_err_t err = esp_http_client_open(fw_client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(fw_client);
        set_failed("could not connect to firmware endpoint");
        return;
    }
    esp_http_client_fetch_headers(fw_client);
    int fw_status = esp_http_client_get_status_code(fw_client);
    if (fw_status < 200 || fw_status >= 300) {
        esp_http_client_close(fw_client);
        esp_http_client_cleanup(fw_client);
        set_failed("firmware download HTTP error");
        return;
    }

    esp_ota_handle_t ota_handle = 0;
    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        esp_http_client_close(fw_client);
        esp_http_client_cleanup(fw_client);
        set_failed("esp_ota_begin failed");
        return;
    }

    char *buf = malloc(DOWNLOAD_BUF_SIZE);
    if (!buf) {
        esp_ota_abort(ota_handle);
        esp_http_client_close(fw_client);
        esp_http_client_cleanup(fw_client);
        set_failed("out of memory");
        return;
    }

    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0 /* SHA-256, not SHA-224 */);

    bool download_ok = true;
    bool write_started_mid_download = false;
    int r;
    while ((r = esp_http_client_read(fw_client, buf, DOWNLOAD_BUF_SIZE)) > 0) {
        if (g_weld_write_active) {
            /* A robot write started after this update began -- same
             * flash/USB-bus-quiescence rule as the check above. */
            download_ok = false;
            write_started_mid_download = true;
            break;
        }
        if (esp_ota_write(ota_handle, buf, r) != ESP_OK) {
            download_ok = false;
            break;
        }
        mbedtls_sha256_update(&sha_ctx, (const unsigned char *)buf, (size_t)r);
    }
    if (r < 0) {
        download_ok = false;
    }
    free(buf);
    esp_http_client_close(fw_client);
    esp_http_client_cleanup(fw_client);

    if (!download_ok) {
        mbedtls_sha256_free(&sha_ctx);
        esp_ota_abort(ota_handle);
        set_failed(write_started_mid_download
                   ? "weld write started mid-update — aborted"
                   : "download interrupted");
        return;
    }

    s_status = OTA_STATUS_VERIFYING;

    unsigned char digest[32];
    mbedtls_sha256_finish(&sha_ctx, digest);
    mbedtls_sha256_free(&sha_ctx);

    char computed_hex[65];
    for (int i = 0; i < 32; i++) {
        snprintf(computed_hex + i * 2, 3, "%02x", digest[i]);
    }

    if (!ota_policy_verify_checksum(computed_hex, fw_checksum, fw_algorithm)) {
        /* Hard precondition (Q19): never mark this partition bootable. Log
         * both values -- neither is secret (a firmware checksum, not a
         * credential) -- since "checksum verification failed" alone gives no
         * way to tell a real corrupt download from an algorithm-name/format
         * mismatch against what ThingsBoard actually advertised. */
        ESP_LOGE(TAG, "Checksum mismatch: computed=%s expected=%s algorithm=%s",
                 computed_hex, fw_checksum, fw_algorithm);
        esp_ota_abort(ota_handle);
        set_failed("checksum verification failed");
        return;
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        /* esp_ota_end() already released the handle on failure -- no abort call. */
        set_failed("firmware image validation failed");
        return;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        set_failed("could not set boot partition");
        return;
    }

    s_status = OTA_STATUS_SUCCESS;
    ESP_LOGI(TAG, "OTA success — rebooting in 3 s");
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
}

static void ota_task(void *pvParam)
{
    while (true) {
        /* Poll the trigger flag — 1 s interval is fine for this use case */
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (!g_ota_trigger) {
            continue;
        }
        g_ota_trigger = false;
        run_update();
    }
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void ota_init(void)
{
    /* run_update() carries ~2.5KB of its own locals (title/version/checksum/
     * URL buffers) live across two sequential HTTPS/TLS calls, on top of
     * mbedtls's own TLS-handshake stack usage (X.509 parse + signature
     * verify -- the same reason webserver.c's httpd worker stack was bumped
     * to 8192 for a *single*, lighter-weight TLS call). Sized generously
     * rather than tightly here since this is a single background task and
     * RAM is not scarce on this board (8MB PSRAM). */
    xTaskCreate(ota_task, "ota", 16384, NULL, 5, NULL);
    ESP_LOGI(TAG, "OTA component initialised");
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
    case OTA_STATUS_VERIFYING:   return "verifying";
    case OTA_STATUS_SUCCESS:     return "success";
    case OTA_STATUS_FAILED:      return "failed";
    default:                     return "unknown";
    }
}

const char *ota_status_reason(void)
{
    return s_status_reason;
}

bool ota_check_update(char *out_version, size_t out_size, bool *out_available)
{
    char tb_token[TB_TOKEN_MAX_LEN];
    char tb_host[TB_HOST_MAX_LEN];
    if (!nvs_read_tb_config(tb_host, sizeof(tb_host), tb_token, sizeof(tb_token))) {
        ESP_LOGW(TAG, "OTA check: no server access token configured");
        return false;
    }

    char resp[512];
    if (!fetch_shared_attributes(tb_host, tb_token, "fw_version", resp, sizeof(resp))) {
        ESP_LOGW(TAG, "OTA check: could not fetch attributes");
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
