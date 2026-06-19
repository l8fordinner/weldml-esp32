#pragma once

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
