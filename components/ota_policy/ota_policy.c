#include "ota_policy.h"

#include <string.h>

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
