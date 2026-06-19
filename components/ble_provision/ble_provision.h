#pragma once

/**
 * @brief Start BLE-based WiFi provisioning.
 *
 * Advertises a BLE GATT service that accepts WiFi credentials from a
 * provisioning app (e.g. the Espressif BLE Provisioning app).
 * Credentials are saved to NVS on success and the device reboots.
 *
 * Only include this component on boards where CONFIG_BOARD_HAS_BLE is set.
 * On boards without BLE this component must not be compiled in.
 *
 * @param service_name  BLE device name advertised during provisioning.
 */
void ble_provision_start(const char *service_name);

/** @brief Stop the BLE provisioning service (call after credentials received). */
void ble_provision_stop(void);
