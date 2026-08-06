#include "weld_cloud.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * Expected epoch ms for "[20/07/16 13:14:14]" at a fixed UTC-6 (Central Standard
 * Time, not DST-aware) offset, computed independently via:
 *   TZ=UTC date -d "2020-07-16 19:14:14" +%s  ->  1594926854
 */
static void test_parse_timestamp_valid(void)
{
    int64_t ms = weld_cloud_parse_timestamp("[20/07/16 13:14:14]");
    assert(ms == 1594926854000LL);
}

/* Cross-checked against a second real fixture's .FSJLOG line (test_data/kawasaki_samples/
 * GAP/IF/l320.fsj) + its matching filesystem mtime:
 *   "[21/07/08 15:14:53]" -> TZ=UTC date -d "2021-07-08 21:14:53" +%s -> 1625778893
 * (computed independently, not derived by the code under test). */
static void test_parse_timestamp_second_fixture(void)
{
    int64_t ms = weld_cloud_parse_timestamp("[21/07/08 15:14:53]");
    assert(ms == 1625778893000LL);
}

static void test_parse_timestamp_malformed(void)
{
    assert(weld_cloud_parse_timestamp("not a timestamp") == -1);
    assert(weld_cloud_parse_timestamp("[20/07/16 13:14:14") == -1);   /* missing ] */
    assert(weld_cloud_parse_timestamp("[20/13/16 13:14:14]") == -1);  /* month 13 */
    assert(weld_cloud_parse_timestamp(NULL) == -1);
}

static void test_build_payload_nothing_to_upload(void)
{
    char out[64];
    uint32_t new_watermark = 999; /* sentinel — must stay unchanged on the nothing-to-upload path */
    size_t n = weld_cloud_build_payload(NULL, 0, 0, out, sizeof(out), &new_watermark);
    assert(n == 0);
    assert(new_watermark == 0);
}

/* Feature values chosen as 1..22 (matching FSJ_FEATURE_* enum order 0..21) so any
 * index/name-mapping bug in the payload builder shows up immediately as a mismatch
 * against a specific, recognizable value rather than blending into noise. */
static void fill_test_row(weld_cloud_row_t *row)
{
    *row = (weld_cloud_row_t){0};
    row->uptime_ms = 12345;
    snprintf(row->source_filename, sizeof(row->source_filename), "l060.fsj");
    snprintf(row->fsj_timestamp, sizeof(row->fsj_timestamp), "[20/07/16 13:14:14]");
    row->predicted_class = 0;
    snprintf(row->label, sizeof(row->label), "NP");
    row->probability_class1 = 0.0f;
    for (int i = 0; i < FSJ_FEATURE_COUNT; i++) {
        row->features[i] = (float)(i + 1);
    }
    row->window_start_row = 1;
    row->window_end_row = 6;
    row->window_count = 6;
    row->parse_ms = 100;
    row->features_ms = 200;
}

static const char *EXPECTED_SINGLE_ROW_PAYLOAD =
    "[{\"ts\":1594926854000,\"values\":{"
    "\"predicted_class\":0,\"pass_flag\":1,\"fail_flag\":0,"
    "\"label\":\"NP\",\"probability_class1\":0,"
    "\"RotationSpeed\":1,\"CWT_DominantScale\":2,\"CWT_EnergyEntropy\":3,"
    "\"CWT_MaxScaleEnergy\":4,\"CWT_MinScaleEnergy\":5,\"CWT_TotalEnergy\":6,"
    "\"ClearanceFactor\":7,\"CrestFactor\":8,\"FFT_DominantFreq\":9,"
    "\"FFT_FrequencyBandwidth\":10,\"FFT_SpectralCentroid\":11,"
    "\"FFT_SpectralFlatness\":12,\"FFT_SpectralSpread\":13,\"ImpulseFactor\":14,"
    "\"MaxForceBelow3mm\":15,\"Mean\":16,\"MinPositionStage3\":17,\"PeakValue\":18,"
    "\"PlungeVelocity\":19,\"RMS\":20,\"ShapeFactor\":21,\"StandardDeviation\":22,"
    "\"uptime_ms\":12345,\"source_filename\":\"l060.fsj\","
    "\"window_start_row\":1,\"window_end_row\":6,\"window_count\":6,"
    "\"parse_ms\":100,\"features_ms\":200"
    "}}]";

static void test_build_payload_single_row(void)
{
    weld_cloud_row_t row;
    fill_test_row(&row);

    char out[2048];
    uint32_t new_watermark = 0;
    size_t n = weld_cloud_build_payload(&row, 1, 0, out, sizeof(out), &new_watermark);

    assert(n == strlen(EXPECTED_SINGLE_ROW_PAYLOAD));
    assert(strcmp(out, EXPECTED_SINGLE_ROW_PAYLOAD) == 0);
    assert(new_watermark == 1);
}

/* Count occurrences of needle in haystack — used to assert exactly N row objects
 * landed in the payload, without re-deriving the full expected JSON a second time. */
static int count_occurrences(const char *haystack, const char *needle)
{
    int count = 0;
    const char *p = haystack;
    size_t needle_len = strlen(needle);
    while ((p = strstr(p, needle)) != NULL) {
        count++;
        p += needle_len;
    }
    return count;
}

static void test_build_payload_respects_watermark(void)
{
    weld_cloud_row_t rows[3];
    for (int i = 0; i < 3; i++) {
        fill_test_row(&rows[i]);
        snprintf(rows[i].source_filename, sizeof(rows[i].source_filename), "row%d.fsj", i);
    }

    char out[4096];
    uint32_t new_watermark = 0;
    /* watermark=1: row0 already uploaded, only row1/row2 should appear */
    size_t n = weld_cloud_build_payload(rows, 3, 1, out, sizeof(out), &new_watermark);

    assert(n > 0);
    assert(new_watermark == 3);
    assert(count_occurrences(out, "\"ts\":") == 2);
    assert(strstr(out, "\"row0.fsj\"") == NULL);
    assert(strstr(out, "\"row1.fsj\"") != NULL);
    assert(strstr(out, "\"row2.fsj\"") != NULL);
}

static void test_build_payload_buffer_too_small(void)
{
    weld_cloud_row_t row;
    fill_test_row(&row);

    char out[8]; /* far too small for a real payload */
    uint32_t new_watermark = 999; /* sentinel — must stay unchanged on failure */
    size_t n = weld_cloud_build_payload(&row, 1, 0, out, sizeof(out), &new_watermark);

    assert(n == 0);
    assert(new_watermark == 999);
}

static void test_check_clear_allowed_everything_uploaded(void)
{
    size_t unsent = 999; /* sentinel — must be set to 0 on the allowed path */
    bool allowed = weld_cloud_check_clear_allowed(3, 3, false, &unsent);

    assert(allowed == true);
    assert(unsent == 0);
}

static void test_check_clear_refused_unsent_rows(void)
{
    size_t unsent = 999;
    /* watermark=1: only row0 uploaded, rows 1 and 2 are unsent */
    bool allowed = weld_cloud_check_clear_allowed(1, 3, false, &unsent);

    assert(allowed == false);
    assert(unsent == 2);
}

static void test_check_clear_force_overrides_unsent_rows(void)
{
    size_t unsent = 999;
    /* watermark=1, row_count=3: 2 unsent, but force=true should override the refusal */
    bool allowed = weld_cloud_check_clear_allowed(1, 3, true, &unsent);

    assert(allowed == true);
    assert(unsent == 2); /* still reported, for UI/audit messaging on the override path */
}

int main(void)
{
    test_parse_timestamp_valid();
    test_parse_timestamp_second_fixture();
    test_parse_timestamp_malformed();
    test_build_payload_nothing_to_upload();
    test_build_payload_single_row();
    test_build_payload_respects_watermark();
    test_build_payload_buffer_too_small();
    test_check_clear_allowed_everything_uploaded();
    test_check_clear_refused_unsent_rows();
    test_check_clear_force_overrides_unsent_rows();
    return 0;
}
