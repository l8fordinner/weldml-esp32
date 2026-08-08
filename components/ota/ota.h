#pragma once

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief OTA update status.
 */
typedef enum {
    OTA_STATUS_IDLE        = 0,
    OTA_STATUS_DOWNLOADING = 1,
    OTA_STATUS_VERIFYING   = 2,
    OTA_STATUS_SUCCESS     = 3,
    OTA_STATUS_FAILED      = 4,
} ota_status_t;

/**
 * @brief Initialise the OTA component and start its trigger-polling task.
 *        Ticket #10: updates are always fetched from ThingsBoard's OTA
 *        package feature (same tb_url/tb_token NVS fields already used for
 *        weld-result uploads and the ticket #8 version check) -- there is
 *        no configurable firmware URL.
 */
void ota_init(void);

/**
 * @brief Trigger an OTA update immediately.
 *        Safe to call from any task.
 */
void ota_trigger(void);

/*
 * True while a USB-MSC write is actively in progress (a robot streaming a
 * .fsj file to the SD card) -- written by weld_processor.c, read by ota.c
 * before starting a flash write. Per CLAUDE.md: "Never ... write to flash
 * during an active MSC session. Flash writes require the USB bus to be
 * quiescent." Owned here (not weld_processor.h) so weld_processor -> ota is
 * a one-directional dependency, matching webserver -> ota, ota already has
 * zero dependencies on either. Same single-writer-then-read volatile-bool
 * convention as g_ota_trigger above.
 */
extern volatile bool g_weld_write_active;

/** @brief Return the current OTA status. */
ota_status_t ota_get_status(void);

/** @brief Return a human-readable OTA status string. */
const char *ota_status_str(void);

/**
 * @brief Return the reason the last OTA attempt failed, or an empty string
 * if the current/last status isn't OTA_STATUS_FAILED. Never NULL.
 */
const char *ota_status_reason(void);

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
