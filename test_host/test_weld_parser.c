/*
 * Host-side parser contract validation for Stage 6A.
 *
 * Compile and run from the repo root:
 *   cd test_host && make && ./test_weld_parser
 *
 * All four fixture files must be present at their documented paths.
 * Exit code 0 = all tests pass; 1 = one or more failures.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "../components/weld_parser/weld_parser.h"

#define PASS "\033[32mPASS\033[0m"
#define FAIL "\033[31mFAIL\033[0m"

static int g_failures = 0;

#define CHECK(cond, msg, ...)                                               \
    do {                                                                    \
        if (!(cond)) {                                                      \
            fprintf(stderr, "  " FAIL "  " msg "\n", ##__VA_ARGS__);       \
            g_failures++;                                                   \
        } else {                                                            \
            fprintf(stderr, "  " PASS "  " msg "\n", ##__VA_ARGS__);       \
        }                                                                   \
    } while (0)

typedef struct {
    const char   *path;
    const char   *label;
    const char   *expected_filename;
    uint32_t      expected_total_rows;
    uint32_t      expected_win_start;
    uint32_t      expected_win_end;
    uint32_t      expected_win_count;
    float         expected_rotate;
    float         expected_sample_rate;
} fixture_t;

/*
 * Window bounds confirmed by awk analysis 2026-06-20:
 *   awk 'NF==16 && $1+0==$1 && $1!="TIME"{ total++; sposm=$5+0; stage=$16+0;
 *        if (!f && sposm>=0) { f=1; ws=total-1 }
 *        if (stage==3) { we=total-1 } }
 *   END { print ws, we, (we-ws+1), total }' <file>
 */
static const fixture_t FIXTURES[] = {
    {
        "../test_data/kawasaki_samples/GAP/NP/l314.fsj",
        "GAP/NP  l314.fsj",
        "l314.fsj",
        2568, 1084, 2244, 1161,
        1800.0f, 500.0f,
    },
    {
        "../test_data/kawasaki_samples/GAP/IF/l320.fsj",
        "GAP/IF  l320.fsj",
        "l320.fsj",
        5036, 1055, 4460, 3406,
        1800.0f, 500.0f,
    },
    {
        "../test_data/kawasaki_samples/LOOCV/NP/l060.fsj",
        "LOOCV/NP l060.fsj",
        "l060.fsj",
        4987, 2562, 4415, 1854,
        1600.0f, 500.0f,
    },
    {
        "../test_data/kawasaki_samples/LOOCV/IF/l046.fsj",
        "LOOCV/IF l046.fsj",
        "l046.fsj",
        2419, 1083, 2095, 1013,
        1400.0f, 500.0f,
    },
};

static void run_fixture(const fixture_t *fx)
{
    fprintf(stderr, "\n=== %s ===\n", fx->label);

    fsj_result_t r;
    fsj_status_t s = fsj_parse_file(fx->path, &r);

    CHECK(s == FSJ_OK,
          "fsj_parse_file returns FSJ_OK (got %s: %s)",
          fsj_status_str(s), r.error_msg);

    if (s != FSJ_OK) {
        /* Cannot check further fields if parse failed */
        return;
    }

    CHECK(strcmp(r.filename, fx->expected_filename) == 0,
          "filename=\"%s\" (expected \"%s\")",
          r.filename, fx->expected_filename);

    CHECK(r.total_rows == fx->expected_total_rows,
          "total_rows=%u (expected %u)",
          r.total_rows, fx->expected_total_rows);

    CHECK(r.window_start_row == fx->expected_win_start,
          "window_start_row=%u (expected %u)",
          r.window_start_row, fx->expected_win_start);

    CHECK(r.window_end_row == fx->expected_win_end,
          "window_end_row=%u (expected %u)",
          r.window_end_row, fx->expected_win_end);

    CHECK(r.window_count == fx->expected_win_count,
          "window_count=%u (expected %u)",
          r.window_count, fx->expected_win_count);

    CHECK(r.window_count == r.window_end_row - r.window_start_row + 1,
          "window_count consistency: %u == %u - %u + 1",
          r.window_count, r.window_end_row, r.window_start_row);

    CHECK(r.meta.rotate_found,
          "rotate_found=true");

    if (r.meta.rotate_found) {
        CHECK(fabsf(r.meta.rotate_rpm - fx->expected_rotate) < 1.0f,
              "rotate_rpm=%.1f (expected %.1f)",
              (double)r.meta.rotate_rpm, (double)fx->expected_rotate);
    }

    CHECK(fabsf(r.sample_rate_hz - fx->expected_sample_rate) < 1.0f,
          "sample_rate_hz=%.1f (expected %.1f)",
          (double)r.sample_rate_hz, (double)fx->expected_sample_rate);

    CHECK(r.window_rows != NULL,
          "window_rows != NULL");

    if (r.window_rows && r.window_count > 0) {
        float first_sposm = r.window_rows[0].cols[FSJ_COL_SPOSM];
        CHECK(first_sposm >= 0.0f,
              "first window row: S.POS.M=%.4f >= 0.0",
              (double)first_sposm);

        float last_stage = r.window_rows[r.window_count - 1].cols[FSJ_COL_STAGE];
        CHECK((int)last_stage == 3,
              "last window row: STAGE=%d == 3", (int)last_stage);
    }

    CHECK(r.meta.timestamp[0] == '[',
          "timestamp starts with '[': \"%s\"",
          r.meta.timestamp);

    fsj_result_free(&r);
    CHECK(r.window_rows == NULL,
          "fsj_result_free clears window_rows pointer");
}

/* Error-path tests: parser must fail explicitly on bad input. */
static void run_error_tests(void)
{
    fprintf(stderr, "\n=== error paths ===\n");

    fsj_result_t r;
    fsj_status_t s;

    /* Non-existent file */
    s = fsj_parse_file("/nonexistent/file.fsj", &r);
    CHECK(s == FSJ_ERR_IO,
          "non-existent file → FSJ_ERR_IO (got %s)", fsj_status_str(s));
    CHECK(r.window_rows == NULL,
          "non-existent: window_rows == NULL");

    /* Empty / non-FSJ content — write a temp file */
    const char *tmp = "/tmp/test_not_fsj.txt";
    FILE *tf = fopen(tmp, "w");
    if (tf) {
        fprintf(tf, "not an fsj file\n");
        fclose(tf);
        s = fsj_parse_file(tmp, &r);
        CHECK(s == FSJ_ERR_FORMAT,
              "non-FSJ file → FSJ_ERR_FORMAT (got %s)", fsj_status_str(s));
        CHECK(r.window_rows == NULL,
              "non-FSJ: window_rows == NULL");
        remove(tmp);
    }
}

int main(void)
{
    fprintf(stderr, "weld_parser Stage 6A contract validation\n");
    fprintf(stderr, "parser version: %s\n", FSJ_PARSER_VERSION);

    size_t n = sizeof(FIXTURES) / sizeof(FIXTURES[0]);
    for (size_t i = 0; i < n; i++) {
        run_fixture(&FIXTURES[i]);
    }
    run_error_tests();

    fprintf(stderr, "\n");
    if (g_failures == 0) {
        fprintf(stderr, PASS " — all checks passed\n");
        return 0;
    }
    fprintf(stderr, FAIL " — %d check(s) failed\n", g_failures);
    return 1;
}
