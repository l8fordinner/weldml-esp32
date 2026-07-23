#include "weld_parser.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void require_close(const char *name, float actual, float expected, float tol)
{
    assert(isfinite(actual));
    if (fabsf(actual - expected) > tol) {
        fprintf(stderr, "%s: actual=%f expected=%f tol=%f\n", name, actual, expected, tol);
        assert(0);
    }
}

static void write_fixture(const char *path)
{
    FILE *f = fopen(path, "w");
    assert(f != NULL);
    fprintf(f, ".* TEST HEADER\n");
    fprintf(f, ".LANGUAGE ENGLISH\n");
    fprintf(f, ".FSJLOG\n");
    fprintf(f, " [26/07/23 10:00:00]\n");
    fprintf(f, " TIME LOADCELL GAP DEF  S.POS.M S.POS.LVDT T.AVE POS7  POS9 ICOM7 IFB7 IFB8  IFB9 VEL7 VEL8 STAGE\n");
    fprintf(f, " 0.000 1.000 0 0 -1.000 0 0 4.000 0 0 0 0 0 0 0 1\n");
    fprintf(f, " 0.002 2.000 0 0 0.000 0 0 3.200 0 0 0 0 0 0 0 2\n");
    fprintf(f, " 0.004 3.000 0 0 0.100 0 0 3.000 0 0 0 0 0 0 0 2\n");
    fprintf(f, " 0.006 4.000 0 0 0.200 0 0 2.750 0 0 0 0 0 0 0 2\n");
    fprintf(f, " 0.008 5.000 0 0 0.300 0 0 2.500 0 0 0 0 0 0 0 2\n");
    fprintf(f, " 0.010 10.000 0 0 0.400 0 0 2.400 0 0 0 0 0 0 0 3\n");
    fprintf(f, " 0.012 20.000 0 0 0.500 0 0 2.200 0 0 0 0 0 0 0 3\n");
    fprintf(f, "***** F_FSJ PROCESSING RESULT *****\n");
    fprintf(f, "STAGE 1 PARAM ROTATE = 1600.00\n");
    fprintf(f, "STAGE 3 PARAM ROTATE = 1800.00\n");
    fprintf(f, ".END\n");
    assert(fclose(f) == 0);
}

int main(void)
{
    const char *path = "/tmp/weldml_feature_fixture.fsj";
    write_fixture(path);

    fsj_result_t parsed;
    fsj_status_t st = fsj_parse_file(path, &parsed);
    assert(st == FSJ_OK);
    assert(parsed.window_start_row == 1);
    assert(parsed.window_end_row == 6);
    assert(parsed.window_count == 6);
    require_close("sample_rate_hz", parsed.sample_rate_hz, 500.0f, 0.01f);

    fsj_features_t features;
    st = fsj_extract_features(&parsed, &features);
    assert(st == FSJ_OK);

    for (uint32_t i = 0; i < FSJ_FEATURE_COUNT; i++) {
        assert(fsj_feature_name(i) != NULL);
        assert(isfinite(features.values[i]));
    }
    assert(strcmp(fsj_feature_name(FSJ_FEATURE_ROTATION_SPEED), "RotationSpeed") == 0);
    assert(strcmp(fsj_feature_name(FSJ_FEATURE_STANDARD_DEVIATION), "StandardDeviation") == 0);

    require_close("RotationSpeed", features.values[FSJ_FEATURE_ROTATION_SPEED], 1600.0f, 0.001f);
    require_close("MaxForceBelow3mm", features.values[FSJ_FEATURE_MAX_FORCE_BELOW_3MM], 20.0f, 0.001f);
    require_close("MinPositionStage3", features.values[FSJ_FEATURE_MIN_POSITION_STAGE3], 2.2f, 0.001f);
    require_close("PlungeVelocity", features.values[FSJ_FEATURE_PLUNGE_VELOCITY], 125.0f, 0.001f);

    require_close("Mean", features.values[FSJ_FEATURE_MEAN], 7.333333f, 0.0005f);
    require_close("RMS", features.values[FSJ_FEATURE_RMS], sqrtf(554.0f / 6.0f), 0.0005f);
    require_close("StandardDeviation", features.values[FSJ_FEATURE_STANDARD_DEVIATION], 6.801960f, 0.0005f);
    require_close("PeakValue", features.values[FSJ_FEATURE_PEAK_VALUE], 20.0f, 0.001f);
    require_close("ShapeFactor", features.values[FSJ_FEATURE_SHAPE_FACTOR],
                  features.values[FSJ_FEATURE_RMS] / 7.333333f, 0.0005f);
    require_close("CrestFactor", features.values[FSJ_FEATURE_CREST_FACTOR],
                  20.0f / features.values[FSJ_FEATURE_RMS], 0.0005f);
    require_close("ImpulseFactor", features.values[FSJ_FEATURE_IMPULSE_FACTOR], 20.0f / 7.333333f, 0.0005f);
    require_close("FFT_DominantFreq", features.values[FSJ_FEATURE_FFT_DOMINANT_FREQ], 46888.550f, 0.05f);
    require_close("FFT_FrequencyBandwidth", features.values[FSJ_FEATURE_FFT_FREQUENCY_BANDWIDTH], 49487.914f, 0.05f);
    require_close("FFT_SpectralCentroid", features.values[FSJ_FEATURE_FFT_SPECTRAL_CENTROID], 78864.750f, 0.05f);
    require_close("FFT_SpectralSpread", features.values[FSJ_FEATURE_FFT_SPECTRAL_SPREAD], 49304.070f, 0.05f);
    require_close("FFT_SpectralFlatness", features.values[FSJ_FEATURE_FFT_SPECTRAL_FLATNESS], 0.685072f, 0.0005f);
    require_close("CWT_DominantScale", features.values[FSJ_FEATURE_CWT_DOMINANT_SCALE], 16.0f, 0.001f);
    require_close("CWT_EnergyEntropy", features.values[FSJ_FEATURE_CWT_ENERGY_ENTROPY], 1.513984f, 0.0005f);
    require_close("CWT_MaxScaleEnergy", features.values[FSJ_FEATURE_CWT_MAX_SCALE_ENERGY], 347.639648f, 0.05f);
    require_close("CWT_MinScaleEnergy", features.values[FSJ_FEATURE_CWT_MIN_SCALE_ENERGY], 10.906071f, 0.05f);
    require_close("CWT_TotalEnergy", features.values[FSJ_FEATURE_CWT_TOTAL_ENERGY], 1172.602051f, 0.05f);

    fsj_result_free(&parsed);
    remove(path);
    return 0;
}
