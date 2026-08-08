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

int main(void)
{
    test_update_available_when_versions_differ();
    test_no_update_when_versions_match();
    test_no_update_when_advertised_empty();
    test_no_update_when_advertised_null();
    test_update_available_when_running_null();
    test_no_update_when_both_null();
    return 0;
}
