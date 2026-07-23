#include "weld_inference.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GOLDEN_CSV
#define GOLDEN_CSV "../../model_exports/esp32_port/golden_vectors/golden_vectors.csv"
#endif
#define MAX_LINE 4096

static const int FEATURE_COLS[FSJ_FEATURE_COUNT] = {
    5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
    23, 24, 25, 26,
};

static void trim_crlf(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[--n] = '\0';
    }
}

static int split_csv_line(char *line, char **fields, int max_fields)
{
    int count = 0;
    char *p = line;
    while (count < max_fields) {
        fields[count++] = p;
        char *comma = strchr(p, ',');
        if (!comma) {
            break;
        }
        *comma = '\0';
        p = comma + 1;
    }
    return count;
}

int main(void)
{
    FILE *f = fopen(GOLDEN_CSV, "r");
    assert(f != NULL);

    char line[MAX_LINE];
    assert(fgets(line, sizeof(line), f) != NULL);

    int rows = 0;
    while (fgets(line, sizeof(line), f)) {
        trim_crlf(line);
        char *fields[32] = {0};
        int count = split_csv_line(line, fields, 32);
        assert(count >= 29);

        fsj_features_t features = {0};
        for (int i = 0; i < FSJ_FEATURE_COUNT; i++) {
            features.values[i] = strtof(fields[FEATURE_COLS[i]], NULL);
            assert(isfinite(features.values[i]));
        }

        int expected_class = atoi(fields[27]);
        float expected_probability_class1 = strtof(fields[28], NULL);

        weld_inference_result_t result = {0};
        assert(weld_inference_predict(&features, &result));
        if (result.predicted_class != expected_class ||
            fabsf(result.probability_class1 - expected_probability_class1) > 0.0001f) {
            fprintf(stderr,
                    "%s: predicted=%d expected=%d probability=%f expected_probability=%f\n",
                    fields[2], result.predicted_class, expected_class,
                    result.probability_class1, expected_probability_class1);
            assert(0);
        }
        assert(strcmp(result.label, expected_class == 1 ? "IF" : "NP") == 0);
        rows++;
    }

    assert(fclose(f) == 0);
    assert(rows == 38);
    return 0;
}
