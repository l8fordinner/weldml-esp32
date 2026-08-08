#pragma once

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief OTA update status.
 */
typedef enum {
    OTA_STATUS_IDLE        = 0,
    OTA_STATUS_DOWNLOADING = 1,
    OTA_STATUS_SUCCESS     = 2,
    OTA_STATUS_FAILED      = 3,
} ota_status_t;

/**
 * @brief Initialise the OTA component and start the monitor task.
 *
 * @param firmware_url  HTTPS URL to poll for firmware updates.
 */
void ota_init(const char *firmware_url);

/**
 * @brief Trigger an OTA update immediately.
 *        Safe to call from any task.
 */
void ota_trigger(void);

/** @brief Return the current OTA status. */
ota_status_t ota_get_status(void);

/** @brief Return a human-readable OTA status string. */
const char *ota_status_str(void);

/**
 * @brief Fetches ThingsBoard's advertised fw_version shared attribute for
 * this device (same tb_token/tb_url NVS fields already used for weld-result
 * uploads) and compares it against the running firmware's own version via
 * ota_policy_update_available(). Writes the advertised version string into
 * out_version (out_size bytes, NUL-terminated) on success.
 *
 * @return true if the fetch succeeded (regardless of whether an update is
 * available -- *out_available reflects that). false on any HTTP/config
 * failure (e.g. no access token configured), leaving out_version untouched.
 */
bool ota_check_update(char *out_version, size_t out_size, bool *out_available);
