#include "weld_cloud.h"

#include <stdio.h>
#include <string.h>

/*
 * Days since 1970-01-01 for a proleptic-Gregorian civil date, per Howard Hinnant's
 * well-known days_from_civil algorithm. Avoids timegm()/mktime() portability
 * differences between the host toolchain and ESP-IDF's newlib.
 */
static int64_t days_from_civil(int y, int m, int d)
{
    y -= (m <= 2);
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int yoe = (int)(y - era * 400);                          /* [0, 399] */
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; /* [0, 365] */
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;          /* [0, 146096] */
    return era * 146097 + doe - 719468;
}

int64_t weld_cloud_parse_timestamp(const char *fsj_timestamp)
{
    if (!fsj_timestamp) {
        return -1;
    }

    int yy, mon, day, hh, mm, ss, consumed = 0;
    /* %n doesn't count toward sscanf's return value, and only executes if every
     * preceding conversion/literal (including the closing ']') actually matched —
     * checking it catches inputs missing the trailing bracket that %d alone would
     * silently accept. */
    if (sscanf(fsj_timestamp, "[%d/%d/%d %d:%d:%d]%n",
               &yy, &mon, &day, &hh, &mm, &ss, &consumed) != 6 || consumed == 0) {
        return -1;
    }
    if (mon < 1 || mon > 12 || day < 1 || day > 31 ||
        hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 59) {
        return -1;
    }

    int64_t days = days_from_civil(2000 + yy, mon, day);
    int64_t naive_epoch_sec = days * 86400 + hh * 3600 + mm * 60 + ss;

    /* fsj_timestamp is local (Central) time; UTC = local - TZ_OFFSET_SEC. */
    int64_t utc_epoch_sec = naive_epoch_sec - WELD_CLOUD_TZ_OFFSET_SEC;
    return utc_epoch_sec * 1000;
}

size_t weld_cloud_build_payload(const weld_cloud_row_t *rows, size_t row_count,
                                 uint32_t watermark,
                                 char *out_json, size_t out_size,
                                 uint32_t *out_new_watermark)
{
    if (watermark >= row_count) {
        *out_new_watermark = watermark;
        return 0;
    }

    size_t pos = 0;

    /* Appends via snprintf, tracking the running offset; returns (via the enclosing
     * function) on truncation or encoding failure rather than emitting a partial
     * payload. */
#define APPEND(...)                                                          \
    do {                                                                     \
        if (pos >= out_size) {                                               \
            return 0;                                                       \
        }                                                                    \
        int wr_ = snprintf(out_json + pos, out_size - pos, __VA_ARGS__);     \
        if (wr_ < 0 || (size_t)wr_ >= out_size - pos) {                      \
            return 0;                                                       \
        }                                                                    \
        pos += (size_t)wr_;                                                  \
    } while (0)

    APPEND("[");
    for (size_t i = watermark; i < row_count; i++) {
        const weld_cloud_row_t *row = &rows[i];

        int64_t ts = weld_cloud_parse_timestamp(row->fsj_timestamp);
        if (ts < 0) {
            return 0; /* malformed source timestamp — abort the whole batch */
        }

        int pass_flag = (row->predicted_class == WELD_INFERENCE_CLASS_NP) ? 1 : 0;
        int fail_flag = (row->predicted_class == WELD_INFERENCE_CLASS_IF) ? 1 : 0;

        APPEND("%s{\"ts\":%lld,\"values\":{",
               (i == watermark) ? "" : ",", (long long)ts);
        APPEND("\"predicted_class\":%d,\"pass_flag\":%d,\"fail_flag\":%d,",
               row->predicted_class, pass_flag, fail_flag);
        APPEND("\"label\":\"%s\",\"probability_class1\":%.9g,",
               row->label, (double)row->probability_class1);

        for (int f = 0; f < FSJ_FEATURE_COUNT; f++) {
            APPEND("\"%s\":%.9g,", fsj_feature_name((uint32_t)f), (double)row->features[f]);
        }

        APPEND("\"uptime_ms\":%u,\"source_filename\":\"%s\",",
               (unsigned)row->uptime_ms, row->source_filename);
        APPEND("\"window_start_row\":%u,\"window_end_row\":%u,\"window_count\":%u,",
               (unsigned)row->window_start_row, (unsigned)row->window_end_row,
               (unsigned)row->window_count);
        APPEND("\"parse_ms\":%u,\"features_ms\":%u",
               (unsigned)row->parse_ms, (unsigned)row->features_ms);
        APPEND("}}");
    }
    APPEND("]");

#undef APPEND

    *out_new_watermark = (uint32_t)row_count;
    return pos;
}

bool weld_cloud_check_clear_allowed(uint32_t watermark, size_t row_count, bool force,
                                     size_t *out_unsent_count)
{
    if (watermark < row_count) {
        *out_unsent_count = row_count - watermark;
        return force;
    }

    *out_unsent_count = 0;
    return true;
}

void weld_cloud_cache_append(weld_cloud_row_t *rows, size_t capacity, size_t *count,
                              const weld_cloud_row_t *new_row)
{
    if (*count >= capacity) {
        memmove(&rows[0], &rows[1], (capacity - 1) * sizeof(rows[0]));
        rows[capacity - 1] = *new_row;
        return;
    }

    rows[*count] = *new_row;
    (*count)++;
}

size_t weld_cloud_build_results_json(const weld_cloud_row_t *rows, size_t row_count,
                                      char *out_json, size_t out_size)
{
    if (row_count == 0) {
        return 0;
    }

    size_t pos = 0;

#define APPEND(...)                                                          \
    do {                                                                     \
        if (pos >= out_size) {                                               \
            return 0;                                                       \
        }                                                                    \
        int wr_ = snprintf(out_json + pos, out_size - pos, __VA_ARGS__);     \
        if (wr_ < 0 || (size_t)wr_ >= out_size - pos) {                      \
            return 0;                                                       \
        }                                                                    \
        pos += (size_t)wr_;                                                  \
    } while (0)

    APPEND("[");
    for (size_t i = 0; i < row_count; i++) {
        const weld_cloud_row_t *row = &rows[i];

        int pass_flag = (row->predicted_class == WELD_INFERENCE_CLASS_NP) ? 1 : 0;
        int fail_flag = (row->predicted_class == WELD_INFERENCE_CLASS_IF) ? 1 : 0;

        APPEND("%s{", (i == 0) ? "" : ",");
        APPEND("\"predicted_class\":%d,\"pass_flag\":%d,\"fail_flag\":%d,",
               row->predicted_class, pass_flag, fail_flag);
        APPEND("\"label\":\"%s\",\"probability_class1\":%.9g,",
               row->label, (double)row->probability_class1);

        for (int f = 0; f < FSJ_FEATURE_COUNT; f++) {
            APPEND("\"%s\":%.9g,", fsj_feature_name((uint32_t)f), (double)row->features[f]);
        }

        APPEND("\"uptime_ms\":%u,\"source_filename\":\"%s\",",
               (unsigned)row->uptime_ms, row->source_filename);
        APPEND("\"fsj_timestamp\":\"%s\",", row->fsj_timestamp);
        APPEND("\"window_start_row\":%u,\"window_end_row\":%u,\"window_count\":%u,",
               (unsigned)row->window_start_row, (unsigned)row->window_end_row,
               (unsigned)row->window_count);
        APPEND("\"parse_ms\":%u,\"features_ms\":%u",
               (unsigned)row->parse_ms, (unsigned)row->features_ms);
        APPEND("}");
    }
    APPEND("]");

#undef APPEND

    return pos;
}
