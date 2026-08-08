#include "ota_policy.h"

#include <assert.h>
#include <stddef.h>

static void test_update_available_when_versions_differ(void)
{
    assert(ota_policy_update_available("e05853e-dirty", "v1.2.0") == true);
}

static void test_no_update_when_versions_match(void)
{
    assert(ota_policy_update_available("v1.2.0", "v1.2.0") == false);
}

static void test_no_update_when_advertised_empty(void)
{
    assert(ota_policy_update_available("v1.2.0", "") == false);
}

static void test_no_update_when_advertised_null(void)
{
    assert(ota_policy_update_available("v1.2.0", NULL) == false);
}

static void test_update_available_when_running_null(void)
{
    /* No known running version but ThingsBoard has a package assigned --
     * treat as available rather than crashing or silently refusing. */
    assert(ota_policy_update_available(NULL, "v1.2.0") == true);
}

static void test_no_update_when_both_null(void)
{
    assert(ota_policy_update_available(NULL, NULL) == false);
}

static void test_checksum_matches(void)
{
    assert(ota_policy_verify_checksum(
        "abc123", "abc123", "SHA256") == true);
}

static void test_checksum_matches_case_insensitively(void)
{
    /* Both the hex digest and the algorithm name. */
    assert(ota_policy_verify_checksum(
        "ABC123", "abc123", "sha256") == true);
}

static void test_checksum_mismatch(void)
{
    assert(ota_policy_verify_checksum(
        "abc123", "def456", "SHA256") == false);
}

static void test_checksum_fails_closed_on_empty_computed(void)
{
    assert(ota_policy_verify_checksum(
        "", "abc123", "SHA256") == false);
}

static void test_checksum_fails_closed_on_null_computed(void)
{
    assert(ota_policy_verify_checksum(
        NULL, "abc123", "SHA256") == false);
}

static void test_checksum_fails_closed_on_empty_expected(void)
{
    /* No advertised checksum at all -- nothing to verify against, so this
     * must never be treated as "verified." */
    assert(ota_policy_verify_checksum(
        "abc123", "", "SHA256") == false);
}

static void test_checksum_fails_closed_on_null_expected(void)
{
    assert(ota_policy_verify_checksum(
        "abc123", NULL, "SHA256") == false);
}

static void test_checksum_fails_closed_on_null_algorithm(void)
{
    assert(ota_policy_verify_checksum(
        "abc123", "abc123", NULL) == false);
}

static void test_checksum_fails_closed_on_unsupported_algorithm(void)
{
    /* This project only ever computes SHA-256 for the download -- an
     * advertised algorithm we don't compute can never be verified, so it
     * must fail closed rather than assume a match is meaningful. */
    assert(ota_policy_verify_checksum(
        "abc123", "abc123", "MD5") == false);
}

int main(void)
{
    test_update_available_when_versions_differ();
    test_no_update_when_versions_match();
    test_no_update_when_advertised_empty();
    test_no_update_when_advertised_null();
    test_update_available_when_running_null();
    test_no_update_when_both_null();
    test_checksum_matches();
    test_checksum_matches_case_insensitively();
    test_checksum_mismatch();
    test_checksum_fails_closed_on_empty_computed();
    test_checksum_fails_closed_on_null_computed();
    test_checksum_fails_closed_on_empty_expected();
    test_checksum_fails_closed_on_null_expected();
    test_checksum_fails_closed_on_null_algorithm();
    test_checksum_fails_closed_on_unsupported_algorithm();
    return 0;
}
