#pragma once

#include <stdbool.h>

#include "weld_parser.h"

#define WELD_INFERENCE_MODEL_ID "model_b_loocv947_coarse_tree"
#define WELD_INFERENCE_CLASS_NP 0
#define WELD_INFERENCE_CLASS_IF 1

typedef struct {
    int predicted_class;
    float probability_class1;
    const char *label;
} weld_inference_result_t;

bool weld_inference_predict(const fsj_features_t *features, weld_inference_result_t *out);

const char *weld_inference_class_label(int predicted_class);
