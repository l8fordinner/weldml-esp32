#include "weld_processor.h"

#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "tusb_msc_storage.h"
#include "lcd_st7789.h"
#include "weld_cloud.h"
#include "weld_inference.h"
#include "weld_parser.h"
#include "webserver.h"

static const char *TAG = "weld_proc";

#define IDLE_WINDOW_MS   5000
#define LCD_TEXT_SCALE   7  /* rotated text runs along the 320px long axis; 7 is the largest
                             * integer scale that fits the longest label (WRITING/PROCESS,
                             * 7 chars) within that axis: 7*(6*7-1)=287 <= 320 */
#define SD_MOUNT         "/sdcard"
#define RESULT_PATH      SD_MOUNT "/weldml_result.json"
#define RESULTS_CSV_PATH SD_MOUNT "/weldml_results.csv"
#define MONITOR_STACK    6144

/*
 * In-memory results cache for #4 (web UI results table) and #5 (ThingsBoard upload).
 * Incremental, never repopulated by re-reading weldml_results.csv (see
 * docs/OPEN_QUESTIONS.md Q23) -- appended to at the same point a row is written to the
 * CSV, evicting the oldest entry past capacity (see weld_cloud_cache_append()).
 * Guarded by s_cache_mutex: written from the monitor_task, read from the HTTP server's
 * task when /api/results is requested -- different FreeRTOS tasks.
 */
#define RESULTS_CACHE_CAPACITY 50
static weld_cloud_row_t s_results_cache[RESULTS_CACHE_CAPACITY];
static size_t s_results_cache_count = 0;
static SemaphoreHandle_t s_cache_mutex = NULL;

/*
 * How many rows have ever been evicted from the front of s_results_cache. The
 * NVS-persisted upload watermark (#5) is a GLOBAL monotonic count of rows
 * uploaded since the cache's inception, not a raw array index -- once eviction
 * starts (past RESULTS_CACHE_CAPACITY), array indices shift, so a raw-index
 * watermark would silently desync (weld_cloud_build_payload() could report
 * "nothing to upload" even with genuinely new unsent rows). Converting
 * global <-> local (array-index) watermark values around this offset keeps
 * uploads correct across eviction. See handler_api_upload().
 */
static uint32_t s_results_cache_evicted = 0;

/* Generous upper bound for RESULTS_CACHE_CAPACITY rows of results JSON (~700-900 bytes/row
 * observed; budgets for longer float representations than the test fixture uses). Heap
 * (PSRAM-backed, see sdkconfig CONFIG_SPIRAM*) rather than stack -- too large to be safe on
 * the HTTP server task's stack. */
#define RESULTS_JSON_BUF_SIZE (64 * 1024)

/* Fixed for this deployment (see docs/THINGSBOARD_SETUP.md) -- only the per-device access
 * token varies, stored via POST /api/config's tb_token field. */
#define THINGSBOARD_HOST "iot.mwe-inc.com"
#define TB_TOKEN_MAX_LEN 128
#define TB_URL_BUF_SIZE  256

/*
 * Write-activity tracking.
 * Written by tud_msc_write10_complete_cb (TinyUSB task).
 * Read by monitor_task (app task).
 * Both are single 32-bit/bool writes — atomic on Xtensa.
 */
static volatile uint32_t s_last_write_ms = 0;
static volatile bool     s_write_seen    = false;

static weld_state_t s_state = WELD_STATE_WAITING;

/*
 * Override TinyUSB weak symbol.  Called from the TinyUSB task after each
 * WRITE10 USB transfer completes (before the deferred SPI write executes).
 * The 5000 ms idle window is more than sufficient for deferred writes to finish.
 */
void tud_msc_write10_complete_cb(uint8_t lun)
{
    (void)lun;
    s_last_write_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    s_write_seen    = true;
}

static void set_state(weld_state_t state)
{
    static const uint16_t color_map[] = {
        [WELD_STATE_WAITING]    = LCD_COLOR_CYAN,
        [WELD_STATE_WRITING]    = LCD_COLOR_WHITE,
        [WELD_STATE_PROCESSING] = LCD_COLOR_BLUE,
        [WELD_STATE_SUCCESS]    = LCD_COLOR_GREEN,
        [WELD_STATE_FAILURE]    = LCD_COLOR_RED,
    };
    static const char *label_map[] = {
        [WELD_STATE_WAITING]    = "READY",
        [WELD_STATE_WRITING]    = "WRITING",
        [WELD_STATE_PROCESSING] = "PROCESS",
        [WELD_STATE_SUCCESS]    = "PASS",
        [WELD_STATE_FAILURE]    = "FAIL",
    };
    s_state = state;
    lcd_st7789_fill(color_map[state]);
    lcd_st7789_draw_text_centered(label_map[state], LCD_COLOR_BLACK, LCD_TEXT_SCALE);
}

/*
 * Scan SD_MOUNT for the most-recently-modified file with extension `ext`
 * (case-insensitive).  Returns true and fills out_path/out_mtime on success.
 */
static bool find_newest(const char *ext, char *out_path, size_t path_len, time_t *out_mtime)
{
    DIR *dir = opendir(SD_MOUNT);
    if (!dir) {
        return false;
    }

    bool found  = false;
    *out_mtime  = 0;
    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type != DT_REG) {
            continue;
        }
        const char *dot = strrchr(ent->d_name, '.');
        if (!dot || strcasecmp(dot + 1, ext) != 0) {
            continue;
        }
        char path[320];
        snprintf(path, sizeof(path), SD_MOUNT "/%s", ent->d_name);

        struct stat st;
        if (stat(path, &st) != 0) {
            continue;
        }
        if (!found || st.st_mtime > *out_mtime) {
            found      = true;
            *out_mtime = st.st_mtime;
            snprintf(out_path, path_len, "%s", path);
        }
    }
    closedir(dir);
    return found;
}

/*
 * Fallback: any regular non-JSON file, excluding our own result file.
 */
static bool find_any_file(char *out_path, size_t path_len, time_t *out_mtime)
{
    DIR *dir = opendir(SD_MOUNT);
    if (!dir) {
        return false;
    }

    bool found = false;
    *out_mtime = 0;
    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type != DT_REG) {
            continue;
        }
        const char *dot = strrchr(ent->d_name, '.');
        if (dot && strcasecmp(dot + 1, "json") == 0) {
            continue;  /* skip JSON files including our own result */
        }
        char path[320];
        snprintf(path, sizeof(path), SD_MOUNT "/%s", ent->d_name);

        struct stat st;
        if (stat(path, &st) != 0) {
            continue;
        }
        if (!found || st.st_mtime > *out_mtime) {
            found      = true;
            *out_mtime = st.st_mtime;
            snprintf(out_path, path_len, "%s", path);
        }
    }
    closedir(dir);
    return found;
}

/*
 * Shared by write_result_json()/write_error_json() so both always agree on the
 * column set -- a mismatched header vs. row field count produces a ragged CSV.
 * 6 fixed columns + FSJ_FEATURE_COUNT features + 5 window/timing columns.
 */
static void write_csv_header(FILE *csv)
{
    fprintf(csv, "uptime_ms,source_filename,status,predicted_class,label,probability_class1");
    for (int f = 0; f < FSJ_FEATURE_COUNT; f++) {
        fprintf(csv, ",%s", fsj_feature_name((uint32_t)f));
    }
    fprintf(csv, ",window_start_row,window_end_row,window_count,parse_ms,features_ms\n");
}

static bool write_result_json(const char *src_path, time_t src_mtime,
                              const fsj_result_t *parsed,
                              const fsj_features_t *features,
                              const weld_inference_result_t *result,
                              uint32_t parse_ms, uint32_t features_ms)
{
    FILE *f = fopen(RESULT_PATH, "w");
    if (!f) {
        ESP_LOGE(TAG, "fopen(%s) failed errno=%d", RESULT_PATH, errno);
        return false;
    }

    /* FAT mtime has 2-second resolution and is not always set by all hosts. */
    char mtime_str[32] = "unknown";
    struct tm tm_info;
    if (src_mtime != 0 && gmtime_r(&src_mtime, &tm_info)) {
        strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%dT%H:%M:%SZ", &tm_info);
    }

    /* Use basename of source path for portability in the JSON output. */
    const char *name = strrchr(src_path, '/');
    name = name ? name + 1 : src_path;

    int wr = fprintf(f,
            "{"
            "\"source\":\"%s\","
            "\"mtime\":\"%s\","
            "\"stage\":6,"
            "\"status\":\"ok\","
            "\"parser_version\":\"%s\","
            "\"model_id\":\"%s\","
            "\"result\":{"
            "\"predicted_class\":%d,"
            "\"label\":\"%s\","
            "\"probability_class1\":%.6f"
            "},"
            "\"parse\":{"
            "\"timestamp\":\"%s\","
            "\"rows\":%" PRIu32 ","
            "\"window_start_row\":%" PRIu32 ","
            "\"window_end_row\":%" PRIu32 ","
            "\"window_count\":%" PRIu32 ","
            "\"sample_rate_hz\":%.3f"
            "},"
            "\"features\":{"
            "\"MinPositionStage3\":%.9g,"
            "\"FFT_FrequencyBandwidth\":%.9g"
            "},"
            "\"timing\":{"
            "\"parse_ms\":%" PRIu32 ","
            "\"features_ms\":%" PRIu32
            "}"
            "}\n",
            name, mtime_str,
            FSJ_PARSER_VERSION,
            WELD_INFERENCE_MODEL_ID,
            result->predicted_class,
            result->label,
            (double)result->probability_class1,
            parsed->meta.timestamp,
            parsed->total_rows,
            parsed->window_start_row,
            parsed->window_end_row,
            parsed->window_count,
            (double)parsed->sample_rate_hz,
            (double)features->values[FSJ_FEATURE_MIN_POSITION_STAGE3],
            (double)features->values[FSJ_FEATURE_FFT_FREQUENCY_BANDWIDTH],
            parse_ms, features_ms);
    bool ok = (wr > 0) && (fflush(f) == 0);
    if (!ok) {
        ESP_LOGE(TAG, "write to %s failed errno=%d", RESULT_PATH, errno);
    }
    if (fclose(f) != 0) {
        ESP_LOGE(TAG, "fclose(%s) failed errno=%d", RESULT_PATH, errno);
        ok = false;
    }
    if (ok) {
        ESP_LOGI(TAG, "Inference: %s -> %s (%s %.3f)",
                 src_path, RESULT_PATH, result->label,
                 (double)result->probability_class1);

        uint32_t uptime_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

        bool need_header = false;
        struct stat csv_st;
        if (stat(RESULTS_CSV_PATH, &csv_st) != 0) {
            need_header = true;
        }
        FILE *csv = fopen(RESULTS_CSV_PATH, "a");
        if (csv) {
            if (need_header) {
                write_csv_header(csv);
            }
            fprintf(csv, "%" PRIu32 ",%s,ok,%d,%s,%.6f",
                    uptime_ms, name, result->predicted_class, result->label,
                    (double)result->probability_class1);
            for (int f = 0; f < FSJ_FEATURE_COUNT; f++) {
                fprintf(csv, ",%.9g", (double)features->values[f]);
            }
            fprintf(csv, ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 "\n",
                    parsed->window_start_row, parsed->window_end_row, parsed->window_count,
                    parse_ms, features_ms);
            fflush(csv);
            fclose(csv);

            /* Append to the in-memory results cache -- same weld-completion event as the
             * CSV write, never repopulated from the CSV (Q23). */
            weld_cloud_row_t cache_row = {0};
            cache_row.uptime_ms = uptime_ms;
            snprintf(cache_row.source_filename, sizeof(cache_row.source_filename), "%s", name);
            snprintf(cache_row.fsj_timestamp, sizeof(cache_row.fsj_timestamp), "%s",
                     parsed->meta.timestamp);
            cache_row.predicted_class = result->predicted_class;
            snprintf(cache_row.label, sizeof(cache_row.label), "%s", result->label);
            cache_row.probability_class1 = result->probability_class1;
            memcpy(cache_row.features, features->values, sizeof(cache_row.features));
            cache_row.window_start_row = parsed->window_start_row;
            cache_row.window_end_row = parsed->window_end_row;
            cache_row.window_count = parsed->window_count;
            cache_row.parse_ms = parse_ms;
            cache_row.features_ms = features_ms;

            if (xSemaphoreTake(s_cache_mutex, portMAX_DELAY) == pdTRUE) {
                if (s_results_cache_count >= RESULTS_CACHE_CAPACITY) {
                    s_results_cache_evicted++;
                }
                weld_cloud_cache_append(s_results_cache, RESULTS_CACHE_CAPACITY,
                                         &s_results_cache_count, &cache_row);
                xSemaphoreGive(s_cache_mutex);
            }
        }
    }
    return ok;
}

static bool write_error_json(const char *src_path, time_t src_mtime,
                             const char *status, const char *detail)
{
    FILE *f = fopen(RESULT_PATH, "w");
    if (!f) {
        ESP_LOGE(TAG, "fopen(%s) failed errno=%d", RESULT_PATH, errno);
        return false;
    }

    char mtime_str[32] = "unknown";
    struct tm tm_info;
    if (src_mtime != 0 && gmtime_r(&src_mtime, &tm_info)) {
        strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%dT%H:%M:%SZ", &tm_info);
    }

    const char *name = NULL;
    if (src_path) {
        name = strrchr(src_path, '/');
        name = name ? name + 1 : src_path;
    }

    int wr;
    if (name) {
        wr = fprintf(f,
                "{\"source\":\"%s\",\"mtime\":\"%s\","
                "\"stage\":6,\"status\":\"%s\",\"error\":\"%s\",\"result\":null}\n",
                name, mtime_str, status, detail);
    } else {
        wr = fprintf(f,
                "{\"source\":null,\"mtime\":null,"
                "\"stage\":6,\"status\":\"%s\",\"error\":\"%s\",\"result\":null}\n",
                status, detail);
    }
    bool ok = (wr > 0) && (fflush(f) == 0);
    if (!ok) {
        ESP_LOGE(TAG, "write to %s failed errno=%d", RESULT_PATH, errno);
    }
    if (fclose(f) != 0) {
        ESP_LOGE(TAG, "fclose(%s) failed errno=%d", RESULT_PATH, errno);
        ok = false;
    }

    bool need_header = false;
    struct stat csv_st;
    if (stat(RESULTS_CSV_PATH, &csv_st) != 0) {
        need_header = true;
    }
    FILE *csv = fopen(RESULTS_CSV_PATH, "a");
    if (csv) {
        if (need_header) {
            write_csv_header(csv);
        }
        /* Same 33-column shape as the success row (write_csv_header/write_result_json) --
         * predicted_class/label/probability_class1 and all FSJ_FEATURE_COUNT feature
         * columns are left empty, since there's no inference result for an error row. */
        fprintf(csv, "%" PRIu32 ",%s,%s,,,,",
                (uint32_t)(esp_timer_get_time() / 1000ULL),
                name ? name : "",
                status);
        for (int f = 0; f < FSJ_FEATURE_COUNT; f++) {
            fprintf(csv, ",");
        }
        fprintf(csv, ",,,,\n");
        fflush(csv);
        fclose(csv);
    }
    return ok;
}

/* Returns predicted class (WELD_INFERENCE_CLASS_NP/IF) or -1 on error. */
static int process_fsj_file(const char *src_path, time_t src_mtime)
{
    uint32_t t0, t1, t2;
    t0 = (uint32_t)(esp_timer_get_time() / 1000ULL);

    fsj_result_t parsed;
    fsj_status_t st = fsj_parse_file(src_path, &parsed);
    if (st != FSJ_OK) {
        ESP_LOGE(TAG, "Parse failed for %s: %s (%s)",
                 src_path, fsj_status_str(st), parsed.error_msg);
        write_error_json(src_path, src_mtime, fsj_status_str(st), parsed.error_msg);
        return -1;
    }
    t1 = (uint32_t)(esp_timer_get_time() / 1000ULL);

    fsj_features_t features;
    st = fsj_extract_features(&parsed, &features);
    if (st != FSJ_OK) {
        ESP_LOGE(TAG, "Feature extraction failed for %s: %s", src_path, fsj_status_str(st));
        fsj_result_free(&parsed);
        write_error_json(src_path, src_mtime, fsj_status_str(st), "feature extraction failed");
        return -1;
    }
    t2 = (uint32_t)(esp_timer_get_time() / 1000ULL);
    uint32_t parse_ms = t1 - t0;
    uint32_t features_ms = t2 - t1;
    ESP_LOGI(TAG, "timing: parse=%lu ms features=%lu ms", (unsigned long)parse_ms, (unsigned long)features_ms);

    weld_inference_result_t result;
    if (!weld_inference_predict(&features, &result)) {
        ESP_LOGE(TAG, "Inference failed for %s", src_path);
        fsj_result_free(&parsed);
        write_error_json(src_path, src_mtime, "inference_failed", "tree inference failed");
        return -1;
    }

    bool ok = write_result_json(src_path, src_mtime, &parsed, &features, &result, parse_ms, features_ms);
    int predicted = result.predicted_class;
    fsj_result_free(&parsed);
    return ok ? predicted : -1;
}

static void process_sd(void)
{
    set_state(WELD_STATE_PROCESSING);

    /*
     * tinyusb_msc_storage_mount() sets is_fat_mounted=true, which causes:
     *   • tud_msc_test_unit_ready_cb → returns false (SCSI "not ready")
     *   • _msc_storage_write_sector  → returns ESP_ERR_INVALID_STATE
     * This gives exclusive ESP access without USB re-enumeration.
     */
    esp_err_t err = tinyusb_msc_storage_mount(SD_MOUNT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "FatFS mount failed: %s", esp_err_to_name(err));
        set_state(WELD_STATE_FAILURE);
        vTaskDelay(pdMS_TO_TICKS(5000));
        set_state(WELD_STATE_WAITING);
        return;  /* mount failed — nothing to unmount */
    }

    char src_path[320] = {0};
    time_t src_mtime   = 0;
    bool found = find_newest("fsj", src_path, sizeof(src_path), &src_mtime);
    if (!found) {
        found = find_any_file(src_path, sizeof(src_path), &src_mtime);
    }

    int prediction;
    if (found) {
        ESP_LOGI(TAG, "Newest file: %s (mtime=%lld)", src_path, (long long)src_mtime);
        prediction = process_fsj_file(src_path, src_mtime);
    } else {
        ESP_LOGW(TAG, "No files on SD card");
        write_error_json(NULL, 0, "no_files", "no weld file found");
        prediction = -1;
    }

    /* Always unmount: returns MSC control to host on both success and failure. */
    tinyusb_msc_storage_unmount();

    /* Blink result 5× at 1 Hz: dark green = PASS (NP), RED = FAIL (IF) or error. */
    bool pass = (prediction == WELD_INFERENCE_CLASS_NP);
    for (int i = 0; i < 5; i++) {
        lcd_st7789_fill(pass ? LCD_COLOR_GREEN : LCD_COLOR_RED);
        lcd_st7789_draw_text_centered(pass ? "PASS" : "FAIL", LCD_COLOR_BLACK, LCD_TEXT_SCALE);
        vTaskDelay(pdMS_TO_TICKS(500));
        lcd_st7789_fill(LCD_COLOR_BLACK);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    set_state(WELD_STATE_WAITING);
}

static void monitor_task(void *arg)
{
    ESP_LOGI(TAG, "Write-idle monitor started — idle window: %d ms", IDLE_WINDOW_MS);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(250));

        if (!s_write_seen) {
            continue;
        }

        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
        uint32_t age_ms = now_ms - s_last_write_ms;

        if (age_ms < IDLE_WINDOW_MS) {
            if (s_state != WELD_STATE_WRITING) {
                set_state(WELD_STATE_WRITING);
            }
        } else {
            /* Idle window elapsed — take SD ownership and process. */
            process_sd();
            s_write_seen = false;
            /* LCD is now GREEN (success) or RED (failure). */
        }
    }
}

esp_err_t weld_processor_start(void)
{
    set_state(WELD_STATE_WAITING);

    s_cache_mutex = xSemaphoreCreateMutex();
    if (!s_cache_mutex) {
        ESP_LOGE(TAG, "Failed to create results cache mutex");
        return ESP_FAIL;
    }

    BaseType_t rc = xTaskCreate(monitor_task, "weld_mon",
                                MONITOR_STACK, NULL, 5, NULL);
    return (rc == pdPASS) ? ESP_OK : ESP_FAIL;
}

/*
 * GET /api/results -- serves the in-memory cache as JSON for the web UI's results
 * table (ticket #4). Runs on the HTTP server's own task, so the cache read is
 * mutex-guarded against monitor_task's writes (see s_cache_mutex).
 */
static esp_err_t handler_api_results(httpd_req_t *req)
{
    char *buf = malloc(RESULTS_JSON_BUF_SIZE);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    size_t n = 0;
    if (xSemaphoreTake(s_cache_mutex, portMAX_DELAY) == pdTRUE) {
        n = weld_cloud_build_results_json(s_results_cache, s_results_cache_count,
                                           buf, RESULTS_JSON_BUF_SIZE);
        xSemaphoreGive(s_cache_mutex);
    }

    httpd_resp_set_type(req, "application/json");
    if (n == 0) {
        /* Nothing cached yet (or the buffer was somehow too small) -- an empty array
         * is a valid response; the web UI just shows an empty table. */
        httpd_resp_sendstr(req, "[]");
    } else {
        httpd_resp_send(req, buf, n);
    }

    free(buf);
    return ESP_OK;
}

/* GET /results -- the results-table page (ticket #4). Product-specific content, so
 * served via weld_processor rather than baked into the generic webserver component. */
static esp_err_t handler_results_page(httpd_req_t *req)
{
    return webserver_serve_file(req, "/web/results.html", "text/html");
}

/*
 * POST /api/upload -- uploads unsent cached rows to ThingsBoard (ticket #5). No
 * request body; always uploads everything not yet marked sent per the NVS watermark.
 * A failed upload (network error or non-2xx) never advances the watermark, so a
 * retry after failure is always safe (no duplicate telemetry, no falsely-marked-sent
 * records) -- per this ticket's explicit acceptance criteria.
 */
static esp_err_t handler_api_upload(httpd_req_t *req)
{
    char tb_token[TB_TOKEN_MAX_LEN] = {0};
    nvs_handle_t nvs;
    if (nvs_open("config", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(tb_token);
        nvs_get_str(nvs, "tb_token", tb_token, &len);
        nvs_close(nvs);
    }

    httpd_resp_set_type(req, "application/json");

    if (tb_token[0] == '\0') {
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"no ThingsBoard access token configured\"}");
        return ESP_OK;
    }

    uint32_t global_watermark = 0;
    if (nvs_open("config", NVS_READONLY, &nvs) == ESP_OK) {
        nvs_get_u32(nvs, "tb_watermark", &global_watermark);
        nvs_close(nvs);
    }

    char *buf = malloc(RESULTS_JSON_BUF_SIZE);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    size_t row_count = 0;
    uint32_t evicted = 0;
    size_t n = 0;
    uint32_t new_local_watermark = 0;
    if (xSemaphoreTake(s_cache_mutex, portMAX_DELAY) == pdTRUE) {
        row_count = s_results_cache_count;
        evicted = s_results_cache_evicted;
        /* Global -> local (array-index) watermark, per the eviction-offset comment
         * above s_results_cache_evicted. Clamped to 0 if the persisted watermark
         * refers to rows already evicted from the cache (those specific unsent rows,
         * if any, are unrecoverable here -- still safe in weldml_results.csv). */
        uint32_t local_watermark = (global_watermark > evicted) ? (global_watermark - evicted) : 0;
        new_local_watermark = (uint32_t)row_count;
        n = weld_cloud_build_payload(s_results_cache, row_count, local_watermark,
                                      buf, RESULTS_JSON_BUF_SIZE, &new_local_watermark);
        xSemaphoreGive(s_cache_mutex);
    }

    if (n == 0) {
        free(buf);
        if ((global_watermark <= evicted ? 0 : global_watermark - evicted) >= row_count) {
            httpd_resp_sendstr(req, "{\"ok\":true,\"uploaded\":0}");
        } else {
            httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"payload too large for buffer\"}");
        }
        return ESP_OK;
    }

    char url[TB_URL_BUF_SIZE];
    snprintf(url, sizeof(url), "https://%s/api/v1/%s/telemetry", THINGSBOARD_HOST, tb_token);

    esp_http_client_config_t http_cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, buf, (int)n);

    esp_err_t err = esp_http_client_perform(client);
    int status = (err == ESP_OK) ? esp_http_client_get_status_code(client) : 0;
    esp_http_client_cleanup(client);
    free(buf);

    char resp[128];
    if (err == ESP_OK && status >= 200 && status < 300) {
        uint32_t new_global_watermark = evicted + new_local_watermark;
        if (nvs_open("config", NVS_READWRITE, &nvs) == ESP_OK) {
            nvs_set_u32(nvs, "tb_watermark", new_global_watermark);
            nvs_commit(nvs);
            nvs_close(nvs);
        }
        unsigned uploaded = (unsigned)(new_global_watermark > global_watermark
                                        ? new_global_watermark - global_watermark : 0);
        snprintf(resp, sizeof(resp), "{\"ok\":true,\"uploaded\":%u}", uploaded);
        httpd_resp_sendstr(req, resp);
        ESP_LOGI(TAG, "Uploaded %u result(s) to ThingsBoard, watermark %" PRIu32 " -> %" PRIu32,
                 uploaded, global_watermark, new_global_watermark);
    } else if (err != ESP_OK) {
        snprintf(resp, sizeof(resp), "{\"ok\":false,\"error\":\"request failed: %s\"}",
                 esp_err_to_name(err));
        httpd_resp_sendstr(req, resp);
        ESP_LOGW(TAG, "ThingsBoard upload failed: %s", esp_err_to_name(err));
    } else {
        snprintf(resp, sizeof(resp), "{\"ok\":false,\"error\":\"HTTP %d\"}", status);
        httpd_resp_sendstr(req, resp);
        ESP_LOGW(TAG, "ThingsBoard upload failed: HTTP %d", status);
    }

    return ESP_OK;
}

esp_err_t weld_processor_register_web_endpoints(void)
{
    static const httpd_uri_t s_results_uri = {
        .uri = "/api/results",
        .method = HTTP_GET,
        .handler = handler_api_results,
    };
    static const httpd_uri_t s_results_page_uri = {
        .uri = "/results",
        .method = HTTP_GET,
        .handler = handler_results_page,
    };
    static const httpd_uri_t s_upload_uri = {
        .uri = "/api/upload",
        .method = HTTP_POST,
        .handler = handler_api_upload,
    };

    esp_err_t err = webserver_register_uri(&s_results_uri);
    if (err != ESP_OK) {
        return err;
    }
    err = webserver_register_uri(&s_results_page_uri);
    if (err != ESP_OK) {
        return err;
    }
    return webserver_register_uri(&s_upload_uri);
}
