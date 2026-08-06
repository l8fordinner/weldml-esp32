#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "weld_parser.h"
#include "weld_inference.h"

/* Central Standard Time, UTC-6. Not DST-aware — see docs/OPEN_QUESTIONS.md. */
#define WELD_CLOUD_TZ_OFFSET_SEC (-6 * 3600)

typedef struct {
    uint32_t uptime_ms;
    char source_filename[64];
    /* Raw fsj timestamp as stored in weld_parser's fsj_meta_t, e.g. "[20/07/16 13:14:14]". */
    char fsj_timestamp[32];
    int predicted_class;
    char label[8];
    float probability_class1;
    float features[FSJ_FEATURE_COUNT];
    uint32_t window_start_row;
    uint32_t window_end_row;
    uint32_t window_count;
    uint32_t parse_ms;
    uint32_t features_ms;
} weld_cloud_row_t;

/*
 * Parse a "[YY/MM/DD HH:MM:SS]" fsj timestamp into epoch milliseconds,
 * applying the fixed WELD_CLOUD_TZ_OFFSET_SEC offset. Returns -1 on parse failure.
 */
int64_t weld_cloud_parse_timestamp(const char *fsj_timestamp);

/*
 * Build the ThingsBoard telemetry JSON payload for rows[watermark..row_count-1]
 * (the rows not yet uploaded). Writes into out_json (out_size bytes, NUL-terminated).
 * Returns the number of bytes written, or 0 if there is nothing to upload or the
 * buffer was too small. On success, *out_new_watermark is set to row_count.
 */
size_t weld_cloud_build_payload(const weld_cloud_row_t *rows, size_t row_count,
                                 uint32_t watermark,
                                 char *out_json, size_t out_size,
                                 uint32_t *out_new_watermark);

/*
 * Determines whether a Clear operation is allowed, per docs/OPEN_QUESTIONS.md Q24.
 * watermark/row_count come from weld_cloud's upload state and the results cache.
 * If force is false and watermark < row_count (unsent rows present), clear is
 * refused and *out_unsent_count reports how many rows are unsent.
 * If force is true, or watermark >= row_count, clear is allowed.
 * Returns true if the caller should proceed with clearing, false if refused.
 */
bool weld_cloud_check_clear_allowed(uint32_t watermark, size_t row_count, bool force,
                                     size_t *out_unsent_count);

/*
 * Appends new_row to rows[] (capacity slots), for weld_processor's bounded
 * in-memory/NVS results cache (see docs/OPEN_QUESTIONS.md Q23 — incremental,
 * never repopulated by re-reading weldml_results.csv). If already at
 * capacity, evicts the oldest entry (shifts rows[1..capacity-1] down to
 * rows[0..capacity-2]) before appending the new row at the end.
 * *count tracks how many of the capacity slots are populated, capped at
 * capacity.
 */
void weld_cloud_cache_append(weld_cloud_row_t *rows, size_t capacity, size_t *count,
                              const weld_cloud_row_t *new_row);

/*
 * Builds a JSON array of all currently-cached rows (oldest first, matching
 * cache order) for the local web UI's results-table endpoint. Unlike
 * weld_cloud_build_payload(), this is NOT watermark-filtered — it always
 * serializes everything currently cached. Writes into out_json (out_size
 * bytes, NUL-terminated). Returns the number of bytes written, or 0 if
 * there is nothing cached or the buffer was too small.
 */
size_t weld_cloud_build_results_json(const weld_cloud_row_t *rows, size_t row_count,
                                      char *out_json, size_t out_size);
