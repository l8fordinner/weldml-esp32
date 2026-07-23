#pragma once

#include <stdint.h>
#include <stdbool.h>

#define FSJ_NUM_COLS       16
#define FSJ_COL_TIME        0
#define FSJ_COL_LOADCELL    1
#define FSJ_COL_GAP         2
#define FSJ_COL_DEF         3
#define FSJ_COL_SPOSM       4   /* S.POS.M — spindle position motor */
#define FSJ_COL_POS7        7   /* POS7 — trainer-selected position column */
#define FSJ_COL_STAGE      15   /* STAGE — 1-5 */

#define FSJ_PARSER_VERSION "1.0.0"
#define FSJ_FEATURE_COUNT  22

typedef enum {
    FSJ_OK         = 0,
    FSJ_ERR_IO     = 1,  /* File open or read failure */
    FSJ_ERR_FORMAT = 2,  /* Missing .FSJLOG keyword, bad column header, etc. */
    FSJ_ERR_WINDOW = 3,  /* No rows with S.POS.M >= 0, or no STAGE==3 rows found */
    FSJ_ERR_NOMEM  = 4,  /* heap allocation failure */
    FSJ_ERR_FEATURE = 5,  /* Feature extraction produced invalid values */
} fsj_status_t;

typedef struct {
    float cols[FSJ_NUM_COLS];
} fsj_row_t;

typedef struct {
    char  timestamp[32];   /* "[YY/MM/DD HH:MM:SS]" from line after .FSJLOG */
    float rotate_rpm;      /* ROTATE value from footer STAGE 3 line; 0.0 if absent */
    bool  rotate_found;
} fsj_meta_t;

typedef struct {
    float max_force_below_3mm;  /* max(LOADCELL where POS7<3 and STAGE in {2,3}) */
    float min_position_stage3;  /* min(POS7 where STAGE==3 and POS7<3) */
    float plunge_velocity;      /* 0.5 / |t(POS7=2.5)-t(POS7=3.0)| in STAGE==2 */
} fsj_stage_metrics_t;

typedef struct {
    fsj_status_t  status;
    char          error_msg[128];
    char          filename[64];      /* basename of the parsed file */
    fsj_meta_t    meta;
    fsj_stage_metrics_t stage_metrics;
    uint32_t      total_rows;        /* total data rows in file */
    uint32_t      window_start_row;  /* 0-based data-row index of first window row */
    uint32_t      window_end_row;    /* 0-based data-row index of last STAGE==3 row */
    uint32_t      window_count;      /* window_end_row - window_start_row + 1 */
    float         sample_rate_hz;    /* 500.0 — confirmed from 0.002 s time step */
    fsj_row_t    *window_rows;       /* heap-allocated; free via fsj_result_free() */
} fsj_result_t;

typedef enum {
    FSJ_FEATURE_ROTATION_SPEED = 0,
    FSJ_FEATURE_CWT_DOMINANT_SCALE,
    FSJ_FEATURE_CWT_ENERGY_ENTROPY,
    FSJ_FEATURE_CWT_MAX_SCALE_ENERGY,
    FSJ_FEATURE_CWT_MIN_SCALE_ENERGY,
    FSJ_FEATURE_CWT_TOTAL_ENERGY,
    FSJ_FEATURE_CLEARANCE_FACTOR,
    FSJ_FEATURE_CREST_FACTOR,
    FSJ_FEATURE_FFT_DOMINANT_FREQ,
    FSJ_FEATURE_FFT_FREQUENCY_BANDWIDTH,
    FSJ_FEATURE_FFT_SPECTRAL_CENTROID,
    FSJ_FEATURE_FFT_SPECTRAL_FLATNESS,
    FSJ_FEATURE_FFT_SPECTRAL_SPREAD,
    FSJ_FEATURE_IMPULSE_FACTOR,
    FSJ_FEATURE_MAX_FORCE_BELOW_3MM,
    FSJ_FEATURE_MEAN,
    FSJ_FEATURE_MIN_POSITION_STAGE3,
    FSJ_FEATURE_PEAK_VALUE,
    FSJ_FEATURE_PLUNGE_VELOCITY,
    FSJ_FEATURE_RMS,
    FSJ_FEATURE_SHAPE_FACTOR,
    FSJ_FEATURE_STANDARD_DEVIATION,
} fsj_feature_index_t;

typedef struct {
    float values[FSJ_FEATURE_COUNT];
} fsj_features_t;

/*
 * Parse an FSJ file and populate *out.
 * On FSJ_OK: window_rows is allocated and must be released via fsj_result_free().
 * On error: window_rows is NULL, error_msg describes the failure.
 */
fsj_status_t fsj_parse_file(const char *path, fsj_result_t *out);

/*
 * Compute the fixed 22-feature model schema from a parsed FSJ file.
 * Features are float32 and follow model_b_loocv947_coarse_tree/feature_order.json.
 */
fsj_status_t fsj_extract_features(const fsj_result_t *parsed, fsj_features_t *out);

const char *fsj_feature_name(uint32_t index);

/* Release heap memory owned by a result.  Safe to call on any result. */
void fsj_result_free(fsj_result_t *result);

const char *fsj_status_str(fsj_status_t s);
