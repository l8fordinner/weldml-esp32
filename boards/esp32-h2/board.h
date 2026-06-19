#pragma once

/* board.h — Generic ESP32-H2
 *
 * Note: H2 has no WiFi.  Products using H2 communicate via Zigbee/Thread
 * and typically pair with a gateway device for cloud connectivity.
 */

#define BOARD_DUAL_CORE       0
#define BOARD_HAS_BT_CLASSIC  0
#define BOARD_HAS_BLE         1
#define BOARD_HAS_WIFI        0
#define BOARD_HAS_NATIVE_USB  0
#define BOARD_HAS_ZIGBEE      1

#define BOARD_LED_PIN         8
#define BOARD_BUTTON_PIN      9

#define BOARD_I2C_SDA_PIN     12
#define BOARD_I2C_SCL_PIN     22

#define BOARD_SPI_MOSI_PIN    5
#define BOARD_SPI_MISO_PIN    0
#define BOARD_SPI_CLK_PIN     4
#define BOARD_SPI_CS_PIN      1
