#include "weld_processor.h"

#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "tusb_msc_storage.h"
#include "lcd_st7789.h"

static const char *TAG = "weld_proc";

#define IDLE_WINDOW_MS  5000
#define SD_MOUNT        "/sdcard"
#define RESULT_PATH     SD_MOUNT "/weldml_result.json"
#define MONITOR_STACK   6144

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
        [WELD_STATE_WRITING]    = LCD_COLOR_YELLOW,
        [WELD_STATE_PROCESSING] = LCD_COLOR_BLUE,
        [WELD_STATE_SUCCESS]    = LCD_COLOR_GREEN,
        [WELD_STATE_FAILURE]    = LCD_COLOR_RED,
    };
    s_state = state;
    lcd_st7789_fill(color_map[state]);
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

static bool write_placeholder(const char *src_path, time_t src_mtime)
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
            "{\"source\":\"%s\",\"mtime\":\"%s\","
            "\"stage\":5,\"status\":\"pending_parse\",\"result\":null}\n",
            name, mtime_str);
    bool ok = (wr > 0) && (fflush(f) == 0);
    if (!ok) {
        ESP_LOGE(TAG, "write to %s failed errno=%d", RESULT_PATH, errno);
    }
    if (fclose(f) != 0) {
        ESP_LOGE(TAG, "fclose(%s) failed errno=%d", RESULT_PATH, errno);
        ok = false;
    }
    if (ok) {
        ESP_LOGI(TAG, "Placeholder: %s -> %s", src_path, RESULT_PATH);
    }
    return ok;
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
        return;  /* mount failed — nothing to unmount */
    }

    char src_path[320] = {0};
    time_t src_mtime   = 0;
    bool found = find_newest("csv", src_path, sizeof(src_path), &src_mtime);
    if (!found) {
        found = find_any_file(src_path, sizeof(src_path), &src_mtime);
    }

    bool write_ok;
    if (found) {
        ESP_LOGI(TAG, "Newest file: %s (mtime=%lld)", src_path, (long long)src_mtime);
        write_ok = write_placeholder(src_path, src_mtime);
    } else {
        ESP_LOGW(TAG, "No files on SD card — writing empty placeholder");
        FILE *f = fopen(RESULT_PATH, "w");
        if (!f) {
            ESP_LOGE(TAG, "fopen(%s) failed errno=%d", RESULT_PATH, errno);
            write_ok = false;
        } else {
            int wr = fprintf(f, "{\"source\":null,\"mtime\":null,"
                               "\"stage\":5,\"status\":\"no_files\",\"result\":null}\n");
            bool ok = (wr > 0) && (fflush(f) == 0);
            if (!ok) {
                ESP_LOGE(TAG, "write to %s failed errno=%d", RESULT_PATH, errno);
            }
            if (fclose(f) != 0) {
                ESP_LOGE(TAG, "fclose(%s) failed errno=%d", RESULT_PATH, errno);
                ok = false;
            }
            write_ok = ok;
        }
    }

    /* Always unmount: returns MSC control to host on both success and failure. */
    tinyusb_msc_storage_unmount();

    set_state(write_ok ? WELD_STATE_SUCCESS : WELD_STATE_FAILURE);
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

    BaseType_t rc = xTaskCreate(monitor_task, "weld_mon",
                                MONITOR_STACK, NULL, 5, NULL);
    return (rc == pdPASS) ? ESP_OK : ESP_FAIL;
}
