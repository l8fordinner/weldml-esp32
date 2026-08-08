#pragma once

#include <stdbool.h>

/*
 * Decides whether an OTA update is available, per docs/OPEN_QUESTIONS.md Q17.
 * Equality-based, not semver-ordered: ThingsBoard's advertised version is
 * whatever OTA package the admin assigned to this device -- the intended
 * target, not necessarily numerically "newer" (a deliberate rollback is a
 * valid admin action). Returns true when advertised_version is non-empty
 * and differs from running_version.
 */
bool ota_policy_update_available(const char *running_version, const char *advertised_version);
