#pragma once

#include <stdbool.h>

/**
 * @brief Bring up WiFi: station mode if credentials are available, SoftAP
 * fallback otherwise.
 *
 * Credentials are read from NVS ("config" namespace, keys "wifi_ssid" /
 * "wifi_pass" — the same keys webserver.c's POST /api/wifi handler writes).
 * If NVS has no SSID, falls back to the CONFIG_WIFI_SSID / CONFIG_WIFI_PASSWORD
 * Kconfig defaults. If the resulting SSID is still empty, or station connect
 * fails/times out, starts a SoftAP (CONFIG_WIFI_AP_SSID etc.) so credentials
 * can be entered via the web UI.
 *
 * Must be called after nvs_flash_init(), esp_netif_init(), and
 * esp_event_loop_create_default().
 *
 * @return true if station mode connected, false if SoftAP fallback was used.
 */
bool wifi_provision_start(void);
