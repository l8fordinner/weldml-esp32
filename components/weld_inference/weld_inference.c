#include "weld_inference.h"

#include <math.h>
#include <stddef.h>

#define MIN_POSITION_STAGE3_THRESHOLD       2.1850000619888306f
#define FFT_FREQUENCY_BANDWIDTH_THRESHOLD  0.24015963822603226f

const char *weld_inference_class_label(int predicted_class)
{
    switch (predicted_class) {
        case WELD_INFERENCE_CLASS_NP:
            return "NP";
        case WELD_INFERENCE_CLASS_IF:
            return "IF";
        default:
            return "UNKNOWN";
    }
}

bool weld_inference_predict(const fsj_features_t *features, weld_inference_result_t *out)
{
    if (!features || !out) {
        return false;
    }

    const float min_position_stage3 =
        features->values[FSJ_FEATURE_MIN_POSITION_STAGE3];
    const float fft_frequency_bandwidth =
        features->values[FSJ_FEATURE_FFT_FREQUENCY_BANDWIDTH];

    if (!isfinite(min_position_stage3) || !isfinite(fft_frequency_bandwidth)) {
        return false;
    }

    if (min_position_stage3 <= MIN_POSITION_STAGE3_THRESHOLD) {
        out->predicted_class = WELD_INFERENCE_CLASS_NP;
        out->probability_class1 = 0.0f;
    } else if (fft_frequency_bandwidth <= FFT_FREQUENCY_BANDWIDTH_THRESHOLD) {
        out->predicted_class = WELD_INFERENCE_CLASS_NP;
        out->probability_class1 = 0.0f;
    } else {
        out->predicted_class = WELD_INFERENCE_CLASS_IF;
        out->probability_class1 = 1.0f;
    }

    out->label = weld_inference_class_label(out->predicted_class);
    return true;
}
