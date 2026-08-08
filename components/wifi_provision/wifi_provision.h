#pragma once

#include <stdbool.h>

/* Max saved networks kept in the try-order list (NVS "config" namespace,
 * "wifi_count" + "wifi_ssidN"/"wifi_passN"). Adding past this cap evicts the
 * oldest entry. */
#define WIFI_PROVISION_MAX_NETWORKS 5

/**
 * @brief Bring up WiFi: station mode if any saved network is available,
 * SoftAP fallback otherwise.
 *
 * Tries saved networks in order (most-recently-added first — see
 * wifi_provision_add_network()). If none connect, or the list is empty,
 * falls back to the CONFIG_WIFI_SSID / CONFIG_WIFI_PASSWORD Kconfig default
 * as a last resort. If that also fails or nothing is configured at all,
 * starts a SoftAP (CONFIG_WIFI_AP_SSID etc.) so credentials can be entered
 * via the web UI.
 *
 * Must be called after nvs_flash_init(), esp_netif_init(), and
 * esp_event_loop_create_default().
 *
 * @return true if station mode connected, false if SoftAP fallback was used.
 */
bool wifi_provision_start(void);

/**
 * @brief Save a network, promoting it to the front of the try-order list
 * (tried first on the next boot/reconnect cycle). Re-adding an SSID already
 * in the list updates its password and moves it to the front instead of
 * creating a duplicate entry. Oldest entry is evicted once
 * WIFI_PROVISION_MAX_NETWORKS is reached.
 */
void wifi_provision_add_network(const char *ssid, const char *pass);

/**
 * @brief Remove a saved network by SSID. Does not disconnect an active
 * connection to it — takes effect on the next reconnect attempt.
 * @return false if the SSID wasn't found in the list.
 */
bool wifi_provision_delete_network(const char *ssid);

/**
 * @brief Fill ssids[][33] (passwords are never exposed) with up to max_out
 * saved SSIDs, most-recently-added first.
 * @return the number of entries filled.
 */
int wifi_provision_list_networks(char ssids[][33], int max_out);
