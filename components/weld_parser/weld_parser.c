#include "weld_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

#ifdef ESP_PLATFORM
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_dsp.h"
#define FSJ_LOGE(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#define FSJ_LOGI(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define FSJ_MS() ((uint32_t)(esp_timer_get_time() / 1000ULL))
#else
#define FSJ_LOGE(tag, fmt, ...) fprintf(stderr, "[E] %s: " fmt "\n", (tag), ##__VA_ARGS__)
#define FSJ_LOGI(tag, fmt, ...) fprintf(stderr, "[I] %s: " fmt "\n", (tag), ##__VA_ARGS__)
#define FSJ_MS() 0
#endif

static const char *TAG = "weld_parser";

#define LINE_BUF        256
#define INITIAL_WIN_CAP 512
#define FFT_PAD_LENGTH  4096
#define CWT_SCALE_COUNT 6
#define CWT_WAVELET_POINTS 4096
#define FEATURE_EPSILON 1.0e-12f

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const float CWT_SCALES[CWT_SCALE_COUNT] = {1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f};

typedef struct {
    bool prev_stage2_valid;
    float prev_stage2_pos;
    float prev_stage2_time;
    bool t30_found;
    bool t25_found;
    float t30;
    float t25;
    bool closest30_found;
    bool closest25_found;
    float closest30_abs_delta;
    float closest25_abs_delta;
    float closest30_time;
    float closest25_time;
    bool max_force_found;
    float max_force_below_3mm;
    bool min_pos_found;
    float min_position_stage3;
} stage_metric_accum_t;

static const char *FEATURE_NAMES[FSJ_FEATURE_COUNT] = {
    "RotationSpeed",
    "CWT_DominantScale",
    "CWT_EnergyEntropy",
    "CWT_MaxScaleEnergy",
    "CWT_MinScaleEnergy",
    "CWT_TotalEnergy",
    "ClearanceFactor",
    "CrestFactor",
    "FFT_DominantFreq",
    "FFT_FrequencyBandwidth",
    "FFT_SpectralCentroid",
    "FFT_SpectralFlatness",
    "FFT_SpectralSpread",
    "ImpulseFactor",
    "MaxForceBelow3mm",
    "Mean",
    "MinPositionStage3",
    "PeakValue",
    "PlungeVelocity",
    "RMS",
    "ShapeFactor",
    "StandardDeviation",
};

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

static float safe_div(float num, float den)
{
    return (fabsf(den) > FEATURE_EPSILON) ? (num / den) : NAN;
}

static float interp_crossing(float p1, float t1, float p2, float t2, float target)
{
    if (p1 == target) {
        return t1;
    }
    if (p2 == target) {
        return t2;
    }
    float ratio = (target - p1) / (p2 - p1);
    return t1 + ratio * (t2 - t1);
}

static void update_closest(float pos, float time, float target,
                           bool *found, float *best_delta, float *best_time)
{
    float delta = fabsf(pos - target);
    if (!*found || delta < *best_delta) {
        *found = true;
        *best_delta = delta;
        *best_time = time;
    }
}

static void update_crossing(stage_metric_accum_t *acc, float pos, float time, float target,
                            bool *target_found, float *target_time)
{
    if (*target_found || !acc->prev_stage2_valid) {
        return;
    }
    float p1 = acc->prev_stage2_pos;
    float t1 = acc->prev_stage2_time;
    float p2 = pos;
    float t2 = time;
    if (p1 == target || p2 == target || (p1 - target) * (p2 - target) < 0.0f) {
        *target_time = interp_crossing(p1, t1, p2, t2, target);
        *target_found = true;
    }
}

static void stage_metrics_update(stage_metric_accum_t *acc, const float v[FSJ_NUM_COLS])
{
    int stage = (int)v[FSJ_COL_STAGE];
    float pos = v[FSJ_COL_POS7];
    float load = v[FSJ_COL_LOADCELL];
    float time = v[FSJ_COL_TIME];

    if ((stage == 2 || stage == 3) && pos < 3.0f) {
        if (!acc->max_force_found || load > acc->max_force_below_3mm) {
            acc->max_force_found = true;
            acc->max_force_below_3mm = load;
        }
    }

    if (stage == 3 && pos < 3.0f) {
        if (!acc->min_pos_found || pos < acc->min_position_stage3) {
            acc->min_pos_found = true;
            acc->min_position_stage3 = pos;
        }
    }

    if (stage == 2) {
        update_crossing(acc, pos, time, 3.0f, &acc->t30_found, &acc->t30);
        update_crossing(acc, pos, time, 2.5f, &acc->t25_found, &acc->t25);
        update_closest(pos, time, 3.0f, &acc->closest30_found,
                       &acc->closest30_abs_delta, &acc->closest30_time);
        update_closest(pos, time, 2.5f, &acc->closest25_found,
                       &acc->closest25_abs_delta, &acc->closest25_time);
        acc->prev_stage2_valid = true;
        acc->prev_stage2_pos = pos;
        acc->prev_stage2_time = time;
    } else {
        acc->prev_stage2_valid = false;
    }
}

static fsj_stage_metrics_t stage_metrics_finish(const stage_metric_accum_t *acc)
{
    fsj_stage_metrics_t metrics = {0};
    float t30 = acc->t30_found ? acc->t30 : acc->closest30_time;
    float t25 = acc->t25_found ? acc->t25 : acc->closest25_time;
    bool have_t30 = acc->t30_found || acc->closest30_found;
    bool have_t25 = acc->t25_found || acc->closest25_found;

    if (have_t30 && have_t25 && t25 != t30) {
        metrics.plunge_velocity = 0.5f / fabsf(t25 - t30);
    }
    if (acc->min_pos_found) {
        metrics.min_position_stage3 = acc->min_position_stage3;
    }
    if (acc->max_force_found) {
        metrics.max_force_below_3mm = acc->max_force_below_3mm;
    }
    return metrics;
}

const char *fsj_status_str(fsj_status_t s)
{
    switch (s) {
        case FSJ_OK:         return "FSJ_OK";
        case FSJ_ERR_IO:     return "FSJ_ERR_IO";
        case FSJ_ERR_FORMAT: return "FSJ_ERR_FORMAT";
        case FSJ_ERR_WINDOW: return "FSJ_ERR_WINDOW";
        case FSJ_ERR_NOMEM:  return "FSJ_ERR_NOMEM";
        case FSJ_ERR_FEATURE: return "FSJ_ERR_FEATURE";
        default:             return "FSJ_ERR_UNKNOWN";
    }
}

const char *fsj_feature_name(uint32_t index)
{
    return (index < FSJ_FEATURE_COUNT) ? FEATURE_NAMES[index] : NULL;
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
    stage_metric_accum_t stage_acc = {0};

    while (fgets(line, sizeof(line), f)) {
        strip_crlf(line);

        /* Footer begins with "***** F_FSJ PROCESSING RESULT *****" */
        if (line[0] == '*') {
            in_footer = true;
        }

        if (in_footer) {
            /*
             * Extract ROTATE from the first footer match, matching trainer export.
             * Format: "STAGE N ... ROTATE = {value}"
             */
            if (!rotate_found) {
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
        stage_metrics_update(&stage_acc, v);

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
    out->stage_metrics    = stage_metrics_finish(&stage_acc);

    FSJ_LOGI(TAG, "%s: rows=%" PRIu32 " window=[%" PRIu32 "..%" PRIu32
             "] count=%" PRIu32 " rotate=%.0f",
             out->filename, out->total_rows,
             out->window_start_row, out->window_end_row,
             out->window_count, (double)rotate_rpm);

    return FSJ_OK;
}

static fsj_status_t compute_time_features(const fsj_result_t *parsed, fsj_features_t *out)
{
    uint32_t n = parsed->window_count;
    if (n == 0) {
        return FSJ_ERR_FEATURE;
    }

    double sum = 0.0;
    double sum_sq = 0.0;
    double sum_abs = 0.0;
    double sum_sqrt_abs = 0.0;
    float peak = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        float x = parsed->window_rows[i].cols[FSJ_COL_LOADCELL];
        float ax = fabsf(x);
        sum += x;
        sum_sq += (double)x * (double)x;
        sum_abs += ax;
        sum_sqrt_abs += sqrtf(ax);
        if (ax > peak) {
            peak = ax;
        }
    }

    float mean = (float)(sum / (double)n);
    float rms = sqrtf((float)(sum_sq / (double)n));
    float stddev = 0.0f;
    if (n > 1) {
        double var_sum = 0.0;
        for (uint32_t i = 0; i < n; i++) {
            double d = (double)parsed->window_rows[i].cols[FSJ_COL_LOADCELL] - (double)mean;
            var_sum += d * d;
        }
        stddev = sqrtf((float)(var_sum / (double)(n - 1)));
    } else {
        stddev = NAN;
    }

    float mean_abs = (float)(sum_abs / (double)n);
    float mean_sqrt_abs = (float)(sum_sqrt_abs / (double)n);

    out->values[FSJ_FEATURE_MEAN] = mean;
    out->values[FSJ_FEATURE_RMS] = rms;
    out->values[FSJ_FEATURE_STANDARD_DEVIATION] = stddev;
    out->values[FSJ_FEATURE_PEAK_VALUE] = peak;
    out->values[FSJ_FEATURE_SHAPE_FACTOR] = safe_div(rms, mean_abs);
    out->values[FSJ_FEATURE_CREST_FACTOR] = safe_div(peak, rms);
    out->values[FSJ_FEATURE_CLEARANCE_FACTOR] = safe_div(peak, mean_sqrt_abs * mean_sqrt_abs);
    out->values[FSJ_FEATURE_IMPULSE_FACTOR] = safe_div(peak, mean_abs);
    return FSJ_OK;
}

static float fft_mean_dt_padded_like_trainer(const fsj_result_t *parsed)
{
    uint32_t n = parsed->window_count;
    if (n < 2) {
        return 1.0f;
    }
    uint32_t used = (n < FFT_PAD_LENGTH) ? n : FFT_PAD_LENGTH;
    float first = parsed->window_rows[0].cols[FSJ_COL_TIME];
    float last = parsed->window_rows[used - 1].cols[FSJ_COL_TIME];
    float mean_dt = (last - first) / (float)(FFT_PAD_LENGTH - 1);
    return (mean_dt > 0.0f) ? mean_dt : 1.0f;
}

static fsj_status_t compute_fft_features(const fsj_result_t *parsed, fsj_features_t *out)
{
    uint32_t n = parsed->window_count;
    if (n == 0) {
        return FSJ_ERR_FEATURE;
    }

    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum += parsed->window_rows[i].cols[FSJ_COL_LOADCELL];
    }
    float mean = sum / (float)n;
    uint32_t used = (n < FFT_PAD_LENGTH) ? n : FFT_PAD_LENGTH;
    float mean_dt = fft_mean_dt_padded_like_trainer(parsed);
    float freq_step = 1.0f / ((float)FFT_PAD_LENGTH * mean_dt);

    float power_sum = 0.0f;
    float weighted_freq_sum = 0.0f;
    float log_power_sum = 0.0f;
    float max_power = -1.0f;
    uint32_t dominant_idx = 0;
    uint32_t power_count = FFT_PAD_LENGTH / 2 + 1;
    float *powers = calloc(power_count, sizeof(float));
    if (!powers) {
        return FSJ_ERR_NOMEM;
    }

#ifdef ESP_PLATFORM
    /* O(N log N) radix-2 FFT via esp-dsp. Interleaved complex buffer [re0,im0,...]. */
    float *fft_buf = calloc(FFT_PAD_LENGTH * 2, sizeof(float));
    if (!fft_buf) {
        free(powers);
        return FSJ_ERR_NOMEM;
    }
    for (uint32_t i = 0; i < used; i++) {
        fft_buf[2 * i] = parsed->window_rows[i].cols[FSJ_COL_LOADCELL] - mean;
    }
    static bool s_fft_init = false;
    if (!s_fft_init) {
        esp_err_t r = dsps_fft2r_init_fc32(NULL, FFT_PAD_LENGTH);
        if (r != ESP_OK && r != ESP_ERR_DSP_REINITIALIZED) {
            free(fft_buf);
            free(powers);
            return FSJ_ERR_FEATURE;
        }
        s_fft_init = true;
    }
    dsps_fft2r_fc32(fft_buf, FFT_PAD_LENGTH);
    dsps_bit_rev_fc32(fft_buf, FFT_PAD_LENGTH);
    for (uint32_t k = 0; k < power_count; k++) {
        float re = fft_buf[2 * k];
        float im = fft_buf[2 * k + 1];
        float power = re * re + im * im;
        powers[k] = power;
        float freq = (float)k * freq_step;
        power_sum += power;
        weighted_freq_sum += freq * power;
        log_power_sum += logf(power + FEATURE_EPSILON);
        if (power > max_power) {
            max_power = power;
            dominant_idx = k;
        }
    }
    free(fft_buf);
#else
    /* Naive O(N²) DFT — retained for host-side tests. */
    float angle_step = -2.0f * (float)M_PI / (float)FFT_PAD_LENGTH;
    for (uint32_t k = 0; k < power_count; k++) {
        float real = 0.0f;
        float imag = 0.0f;
        float base_angle = angle_step * (float)k;
        for (uint32_t t = 0; t < used; t++) {
            float x = parsed->window_rows[t].cols[FSJ_COL_LOADCELL] - mean;
            float angle = base_angle * (float)t;
            real += x * cosf(angle);
            imag += x * sinf(angle);
        }
        float power = real * real + imag * imag;
        powers[k] = power;
        float freq = (float)k * freq_step;
        power_sum += power;
        weighted_freq_sum += freq * power;
        log_power_sum += logf(power + FEATURE_EPSILON);
        if (power > max_power) {
            max_power = power;
            dominant_idx = k;
        }
    }
#endif

    float dominant_freq = (float)dominant_idx * freq_step;
    float centroid = (power_sum > 0.0f) ? (weighted_freq_sum / power_sum) : NAN;
    float spread_sum = 0.0f;
    if (power_sum > 0.0f && isfinite(centroid)) {
        for (uint32_t k = 0; k < power_count; k++) {
            float freq = (float)k * freq_step;
            float d = freq - centroid;
            spread_sum += d * d * powers[k];
        }
    }
    float spread = (power_sum > 0.0f) ? sqrtf(spread_sum / power_sum) : NAN;

    float half_power = max_power / 2.0f;
    uint32_t first_above = 0;
    uint32_t last_above = 0;
    bool above_found = false;
    for (uint32_t k = 0; k < power_count; k++) {
        if (powers[k] >= half_power) {
            if (!above_found) {
                first_above = k;
                above_found = true;
            }
            last_above = k;
        }
    }
    float bandwidth = above_found ? ((float)(last_above - first_above) * freq_step) : 0.0f;
    float mean_power = power_sum / (float)power_count;
    float flatness = expf(log_power_sum / (float)power_count) / (mean_power + FEATURE_EPSILON);

    out->values[FSJ_FEATURE_FFT_DOMINANT_FREQ] = dominant_freq;
    out->values[FSJ_FEATURE_FFT_SPECTRAL_CENTROID] = centroid;
    out->values[FSJ_FEATURE_FFT_SPECTRAL_SPREAD] = spread;
    out->values[FSJ_FEATURE_FFT_FREQUENCY_BANDWIDTH] = bandwidth;
    out->values[FSJ_FEATURE_FFT_SPECTRAL_FLATNESS] = flatness;
    free(powers);
    return FSJ_OK;
}

static float morlet_integral_sample(uint32_t idx)
{
    static bool initialized = false;
    static float integral[CWT_WAVELET_POINTS];
    if (!initialized) {
        float step = 16.0f / (float)(CWT_WAVELET_POINTS - 1);
        float sum = 0.0f;
        for (uint32_t i = 0; i < CWT_WAVELET_POINTS; i++) {
            float x = -8.0f + (float)i * step;
            float psi = expf(-(x * x) * 0.5f) * cosf(5.0f * x);
            sum += psi * step;
            integral[i] = sum;
        }
        initialized = true;
    }
    return integral[idx];
}

static uint32_t cwt_reversed_wavelet_index(uint32_t reversed_sample, float scale)
{
    float step = 16.0f / (float)(CWT_WAVELET_POINTS - 1);
    uint32_t idx = (uint32_t)floorf((float)reversed_sample / (scale * step));
    return (idx < CWT_WAVELET_POINTS) ? idx : (CWT_WAVELET_POINTS - 1);
}

static fsj_status_t compute_cwt_features(const fsj_result_t *parsed, fsj_features_t *out)
{
    uint32_t n = parsed->window_count;
    if (n == 0) {
        return FSJ_ERR_FEATURE;
    }

    float total_energy = 0.0f;
    float scale_energies[CWT_SCALE_COUNT] = {0};

    for (uint32_t s = 0; s < CWT_SCALE_COUNT; s++) {
        float scale = CWT_SCALES[s];
        uint32_t kernel_len = (uint32_t)floorf(scale * 16.0f) + 1;
        uint32_t conv_len = n + kernel_len - 1;
        uint32_t coef_len = (conv_len > 0) ? conv_len - 1 : 0;
        float trim = ((float)coef_len - (float)n) * 0.5f;
        uint32_t start = (uint32_t)floorf(trim);
        float sqrt_scale = sqrtf(scale);

        for (uint32_t out_idx = 0; out_idx < n; out_idx++) {
            uint32_t coef_idx = start + out_idx;
            float conv0 = 0.0f;
            float conv1 = 0.0f;
            for (uint32_t m = 0; m < kernel_len; m++) {
                if (coef_idx >= m) {
                    uint32_t data_idx = coef_idx - m;
                    if (data_idx < n) {
                        uint32_t base_idx = cwt_reversed_wavelet_index(kernel_len - 1 - m, scale);
                        conv0 += parsed->window_rows[data_idx].cols[FSJ_COL_LOADCELL]
                               * morlet_integral_sample(base_idx);
                    }
                }
                if (coef_idx + 1 >= m) {
                    uint32_t data_idx = coef_idx + 1 - m;
                    if (data_idx < n) {
                        uint32_t base_idx = cwt_reversed_wavelet_index(kernel_len - 1 - m, scale);
                        conv1 += parsed->window_rows[data_idx].cols[FSJ_COL_LOADCELL]
                               * morlet_integral_sample(base_idx);
                    }
                }
            }
            float coef = -sqrt_scale * (conv1 - conv0);
            scale_energies[s] += coef * coef;
        }
        total_energy += scale_energies[s];
    }

    if (total_energy == 0.0f) {
        out->values[FSJ_FEATURE_CWT_TOTAL_ENERGY] = 0.0f;
        out->values[FSJ_FEATURE_CWT_DOMINANT_SCALE] = NAN;
        out->values[FSJ_FEATURE_CWT_ENERGY_ENTROPY] = NAN;
        out->values[FSJ_FEATURE_CWT_MAX_SCALE_ENERGY] = 0.0f;
        out->values[FSJ_FEATURE_CWT_MIN_SCALE_ENERGY] = 0.0f;
        return FSJ_OK;
    }

    uint32_t dominant = 0;
    float max_energy = scale_energies[0];
    float min_energy = scale_energies[0];
    float entropy = 0.0f;
    for (uint32_t s = 0; s < CWT_SCALE_COUNT; s++) {
        if (scale_energies[s] > max_energy) {
            max_energy = scale_energies[s];
            dominant = s;
        }
        if (scale_energies[s] < min_energy) {
            min_energy = scale_energies[s];
        }
        float norm = scale_energies[s] / total_energy;
        entropy += -norm * logf(norm + FEATURE_EPSILON);
    }

    out->values[FSJ_FEATURE_CWT_TOTAL_ENERGY] = total_energy;
    out->values[FSJ_FEATURE_CWT_DOMINANT_SCALE] = CWT_SCALES[dominant];
    out->values[FSJ_FEATURE_CWT_ENERGY_ENTROPY] = entropy;
    out->values[FSJ_FEATURE_CWT_MAX_SCALE_ENERGY] = max_energy;
    out->values[FSJ_FEATURE_CWT_MIN_SCALE_ENERGY] = min_energy;
    return FSJ_OK;
}

fsj_status_t fsj_extract_features(const fsj_result_t *parsed, fsj_features_t *out)
{
    if (!parsed || !out || parsed->status != FSJ_OK || !parsed->window_rows ||
        parsed->window_count == 0 || !parsed->meta.rotate_found) {
        return FSJ_ERR_FEATURE;
    }

    memset(out, 0, sizeof(*out));
    out->values[FSJ_FEATURE_ROTATION_SPEED] = parsed->meta.rotate_rpm;
    out->values[FSJ_FEATURE_MAX_FORCE_BELOW_3MM] = parsed->stage_metrics.max_force_below_3mm;
    out->values[FSJ_FEATURE_MIN_POSITION_STAGE3] = parsed->stage_metrics.min_position_stage3;
    out->values[FSJ_FEATURE_PLUNGE_VELOCITY] = parsed->stage_metrics.plunge_velocity;

    uint32_t t0, t1;
    t0 = FSJ_MS();
    if (compute_time_features(parsed, out) != FSJ_OK) { return FSJ_ERR_FEATURE; }
    t1 = FSJ_MS(); FSJ_LOGI(TAG, "time_features: %lu ms", (unsigned long)(t1 - t0));

    t0 = FSJ_MS();
    if (compute_fft_features(parsed, out) != FSJ_OK) { return FSJ_ERR_FEATURE; }
    t1 = FSJ_MS(); FSJ_LOGI(TAG, "fft_features:  %lu ms", (unsigned long)(t1 - t0));

    t0 = FSJ_MS();
    if (compute_cwt_features(parsed, out) != FSJ_OK) { return FSJ_ERR_FEATURE; }
    t1 = FSJ_MS(); FSJ_LOGI(TAG, "cwt_features:  %lu ms", (unsigned long)(t1 - t0));

    for (uint32_t i = 0; i < FSJ_FEATURE_COUNT; i++) {
        if (!isfinite(out->values[i])) {
            return FSJ_ERR_FEATURE;
        }
    }
    return FSJ_OK;
}
