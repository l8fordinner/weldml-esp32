#include "ota_policy.h"

#include <string.h>
#include <strings.h>

bool ota_policy_update_available(const char *running_version, const char *advertised_version)
{
    if (!advertised_version || advertised_version[0] == '\0') {
        return false;
    }
    if (!running_version) {
        return true;
    }
    return strcmp(running_version, advertised_version) != 0;
}

bool ota_policy_verify_checksum(const char *computed_checksum_hex,
                                 const char *expected_checksum_hex,
                                 const char *checksum_algorithm)
{
    if (!computed_checksum_hex || computed_checksum_hex[0] == '\0') {
        return false;
    }
    if (!expected_checksum_hex || expected_checksum_hex[0] == '\0') {
        return false;
    }
    if (!checksum_algorithm || strcasecmp(checksum_algorithm, "SHA256") != 0) {
        return false;
    }
    return strcasecmp(computed_checksum_hex, expected_checksum_hex) == 0;
}
