/*
 * zigbee.c — Optional Zigbee component stub.
 *
 * Valid only on ESP32-H2 and ESP32-C6 which have a native 802.15.4 radio.
 * On other chips this component must not be compiled in.
 *
 * Flesh this out when a product requires Zigbee.
 * See the Espressif Zigbee SDK examples:
 * $IDF_PATH/examples/zigbee/
 */

#include "esp_log.h"
#include "zigbee.h"

static const char *TAG = "zigbee";

void zigbee_init(void)
{
    /* TODO: Initialise esp_zigbee_core stack.
     *       See $IDF_PATH/examples/zigbee/light_bulb for a reference.
     */
    ESP_LOGW(TAG, "Zigbee stub — not yet implemented");
}

void zigbee_start(void)
{
    ESP_LOGW(TAG, "Zigbee stub — start called");
}
