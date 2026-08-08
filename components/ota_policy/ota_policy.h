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

/*
 * Verifies a downloaded firmware image's computed checksum against ThingsBoard's
 * advertised value, per docs/OPEN_QUESTIONS.md Q19: a hard precondition before the
 * image may ever be marked bootable -- if this returns false, the image must never
 * be flashed as bootable, full stop, not "flash it anyway and rely on rollback to
 * catch it." Fails closed (returns false) on any missing value or unsupported
 * algorithm: this project only ever computes SHA-256 for the download, so any
 * other advertised algorithm can never actually be verified and must not be
 * trusted blindly. Case-insensitive on both the hex digest and algorithm name.
 */
bool ota_policy_verify_checksum(const char *computed_checksum_hex,
                                 const char *expected_checksum_hex,
                                 const char *checksum_algorithm);
