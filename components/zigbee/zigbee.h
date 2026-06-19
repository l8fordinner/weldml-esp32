#pragma once

/**
 * @brief Initialise the Zigbee stack.
 *
 * Valid only on ESP32-H2 and ESP32-C6.
 * Guard calls to this function behind CONFIG_BOARD_HAS_ZIGBEE in your
 * application code.
 */
void zigbee_init(void);

/** @brief Start the Zigbee stack and begin network formation/joining. */
void zigbee_start(void);
