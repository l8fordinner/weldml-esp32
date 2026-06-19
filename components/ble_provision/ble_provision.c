/*
 * ble_provision.c — Optional BLE WiFi provisioning component.
 *
 * Uses Espressif's wifi_provisioning + protocomm libraries.
 * Include this component only on boards with BLE support
 * (CONFIG_BOARD_HAS_BLE=y in sdkconfig.defaults).
 *
 * This is a stub — flesh it out when a product requires BLE provisioning.
 * The Espressif wifi_provisioning example is the recommended starting point:
 * $IDF_PATH/examples/provisioning/wifi_prov_mgr
 */

#include "esp_log.h"
#include "ble_provision.h"

static const char *TAG = "ble_provision";

void ble_provision_start(const char *service_name)
{
    /* TODO: Implement using wifi_provisioning / protocomm.
     *       See $IDF_PATH/examples/provisioning/wifi_prov_mgr for a complete
     *       reference implementation.
     *       This stub logs a warning so it is easy to spot during development.
     */
    ESP_LOGW(TAG, "BLE provisioning stub — not yet implemented. "
             "Service name: %s", service_name ? service_name : "(null)");
}

void ble_provision_stop(void)
{
    ESP_LOGW(TAG, "BLE provisioning stub — stop called");
}
