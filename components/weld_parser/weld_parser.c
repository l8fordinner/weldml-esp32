#include "weld_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

#ifdef ESP_PLATFORM
#include "esp_log.h"
#define FSJ_LOGE(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#define FSJ_LOGI(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#else
#define FSJ_LOGE(tag, fmt, ...) fprintf(stderr, "[E] %s: " fmt "\n", (tag), ##__VA_ARGS__)
#define FSJ_LOGI(tag, fmt, ...) fprintf(stderr, "[I] %s: " fmt "\n", (tag), ##__VA_ARGS__)
#endif

static const char *TAG = "weld_parser";

#define LINE_BUF        256
#define INITIAL_WIN_CAP 512

/* Strip trailing \r and \n from a mutable string. */
static void strip_crlf(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n')) {
        s[--n] = '\0';
    }
}

/* Extract basename (portion after last '/') without modifying the input. */
static const char *basename_of(const char *path)
{
    const char *p = strrchr(path, '/');
    return p ? p + 1 : path;
}

/*
 * Append one row to the window buffer, growing it as needed.
 * Returns false on allocation failure.
 */
static bool buf_append(fsj_row_t **buf, uint32_t *count, uint32_t *cap,
                       const fsj_row_t *row)
{
    if (*count == *cap) {
        uint32_t new_cap = (*cap == 0) ? INITIAL_WIN_CAP : (*cap * 2);
        fsj_row_t *nb = realloc(*buf, new_cap * sizeof(fsj_row_t));
        if (!nb) {
            return false;
        }
        *buf = nb;
        *cap = new_cap;
    }
    (*buf)[(*count)++] = *row;
    return true;
}

const char *fsj_status_str(fsj_status_t s)
{
    switch (s) {
        case FSJ_OK:         return "FSJ_OK";
        case FSJ_ERR_IO:     return "FSJ_ERR_IO";
        case FSJ_ERR_FORMAT: return "FSJ_ERR_FORMAT";
        case FSJ_ERR_WINDOW: return "FSJ_ERR_WINDOW";
        case FSJ_ERR_NOMEM:  return "FSJ_ERR_NOMEM";
        default:             return "FSJ_ERR_UNKNOWN";
    }
}

void fsj_result_free(fsj_result_t *result)
{
    if (result && result->window_rows) {
        free(result->window_rows);
        result->window_rows = NULL;
    }
}

fsj_status_t fsj_parse_file(const char *path, fsj_result_t *out)
{
    memset(out, 0, sizeof(*out));
    out->status = FSJ_ERR_FORMAT;

    strncpy(out->filename, basename_of(path), sizeof(out->filename) - 1);

    FILE *f = fopen(path, "r");
    if (!f) {
        out->status = FSJ_ERR_IO;
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "fopen failed: %s", path);
        FSJ_LOGE(TAG, "%s", out->error_msg);
        return out->status;
    }

    char line[LINE_BUF];

    /* ---- Phase 1: scan header until .FSJLOG is found ---- */
    bool found_fsjlog = false;
    while (fgets(line, sizeof(line), f)) {
        strip_crlf(line);
        if (strcmp(line, ".FSJLOG") == 0) {
            found_fsjlog = true;
            break;
        }
        /* All valid header lines start with '.' or '*'; bail if we see something else
         * after a reasonable number of lines to avoid runaway on malformed files. */
    }
    if (!found_fsjlog) {
        snprintf(out->error_msg, sizeof(out->error_msg), "missing .FSJLOG keyword");
        FSJ_LOGE(TAG, "%s: %s", out->filename, out->error_msg);
        fclose(f);
        return out->status;
    }

    /* ---- Phase 2: timestamp line ---- */
    if (!fgets(line, sizeof(line), f)) {
        snprintf(out->error_msg, sizeof(out->error_msg), "missing timestamp after .FSJLOG");
        FSJ_LOGE(TAG, "%s: %s", out->filename, out->error_msg);
        fclose(f);
        return out->status;
    }
    strip_crlf(line);
    /* Timestamp format: " [YY/MM/DD HH:MM:SS]" — copy trimmed */
    const char *ts = line;
    while (*ts == ' ') ts++;
    snprintf(out->meta.timestamp, sizeof(out->meta.timestamp),
             "%.*s", (int)(sizeof(out->meta.timestamp) - 1), ts);

    /* ---- Phase 3: column header line ---- */
    if (!fgets(line, sizeof(line), f)) {
        snprintf(out->error_msg, sizeof(out->error_msg), "missing column header");
        FSJ_LOGE(TAG, "%s: %s", out->filename, out->error_msg);
        fclose(f);
        return out->status;
    }
    strip_crlf(line);
    if (strstr(line, "S.POS.M") == NULL || strstr(line, "STAGE") == NULL) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "column header missing S.POS.M or STAGE");
        FSJ_LOGE(TAG, "%s: %s", out->filename, out->error_msg);
        fclose(f);
        return out->status;
    }

    /* ---- Phase 4: data rows ---- */
    fsj_row_t   *win_buf     = NULL;
    uint32_t     win_count   = 0;
    uint32_t     win_cap     = 0;
    uint32_t     data_row    = 0;      /* 0-based index across all data rows */
    bool         in_window   = false;
    uint32_t     win_start   = 0;
    uint32_t     last_s3_buf = 0;      /* buffer index of last STAGE==3 row */
    bool         s3_found    = false;
    bool         in_footer   = false;
    float        rotate_rpm  = 0.0f;
    bool         rotate_found = false;
    float        prev_dt     = 0.0f;   /* for sample rate detection */

    while (fgets(line, sizeof(line), f)) {
        strip_crlf(line);

        /* Footer begins with "***** F_FSJ PROCESSING RESULT *****" */
        if (line[0] == '*') {
            in_footer = true;
        }

        if (in_footer) {
            /*
             * Extract ROTATE from STAGE 3 footer line.
             * Format: "STAGE 3 ... ROTATE = {value}"
             */
            if (strncmp(line, "STAGE 3 ", 8) == 0) {
                const char *p = strstr(line, "ROTATE = ");
                if (p) {
                    float r;
                    if (sscanf(p, "ROTATE = %f", &r) == 1) {
                        rotate_rpm   = r;
                        rotate_found = true;
                    }
                }
            }
            if (strcmp(line, ".END") == 0) {
                break;
            }
            continue;
        }

        /* Parse data row: 16 space-delimited floats */
        float v[FSJ_NUM_COLS];
        int n = sscanf(line,
            " %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
            &v[0],  &v[1],  &v[2],  &v[3],
            &v[4],  &v[5],  &v[6],  &v[7],
            &v[8],  &v[9],  &v[10], &v[11],
            &v[12], &v[13], &v[14], &v[15]);
        if (n != FSJ_NUM_COLS) {
            /* Not a data row (might be blank or extra line); skip silently. */
            continue;
        }

        /* Detect sample rate from first two data rows */
        if (data_row == 1) {
            float dt = v[FSJ_COL_TIME] - prev_dt;
            if (dt > 0.0f) {
                out->sample_rate_hz = 1.0f / dt;
            }
        } else if (data_row == 0) {
            prev_dt = v[FSJ_COL_TIME];
        }

        float sposm = v[FSJ_COL_SPOSM];
        float stage = v[FSJ_COL_STAGE];

        /* Window start: first row where S.POS.M >= 0 */
        if (!in_window && sposm >= 0.0f) {
            in_window = true;
            win_start = data_row;
        }

        if (in_window) {
            fsj_row_t row;
            memcpy(row.cols, v, sizeof(v));
            if (!buf_append(&win_buf, &win_count, &win_cap, &row)) {
                free(win_buf);
                out->status = FSJ_ERR_NOMEM;
                snprintf(out->error_msg, sizeof(out->error_msg),
                         "out of memory at data row %" PRIu32, data_row);
                FSJ_LOGE(TAG, "%s: %s", out->filename, out->error_msg);
                fclose(f);
                return out->status;
            }
            /* Track the last STAGE==3 row in the buffer */
            if ((int)stage == 3) {
                last_s3_buf = win_count - 1;
                s3_found    = true;
            }
        }

        data_row++;
    }

    fclose(f);
    out->total_rows = data_row;

    /* Validate window */
    if (!in_window || !s3_found) {
        free(win_buf);
        out->status = FSJ_ERR_WINDOW;
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "no weld window found (SPOSM>=0: %s, STAGE3: %s)",
                 in_window ? "yes" : "no", s3_found ? "yes" : "no");
        FSJ_LOGE(TAG, "%s: %s", out->filename, out->error_msg);
        return out->status;
    }

    /* Truncate buffer to last STAGE==3 row inclusive */
    uint32_t final_count = last_s3_buf + 1;
    if (final_count < win_count) {
        fsj_row_t *trimmed = realloc(win_buf, final_count * sizeof(fsj_row_t));
        if (trimmed) {
            win_buf = trimmed;
        }
        /* If realloc fails to shrink, existing buffer is still valid at win_cap size. */
    }

    /* Sample rate fallback */
    if (out->sample_rate_hz <= 0.0f) {
        out->sample_rate_hz = 500.0f;
    }

    out->status           = FSJ_OK;
    out->window_start_row = win_start;
    out->window_end_row   = win_start + last_s3_buf;
    out->window_count     = final_count;
    out->window_rows      = win_buf;
    out->meta.rotate_rpm  = rotate_rpm;
    out->meta.rotate_found = rotate_found;

    FSJ_LOGI(TAG, "%s: rows=%" PRIu32 " window=[%" PRIu32 "..%" PRIu32
             "] count=%" PRIu32 " rotate=%.0f",
             out->filename, out->total_rows,
             out->window_start_row, out->window_end_row,
             out->window_count, (double)rotate_rpm);

    return FSJ_OK;
}
