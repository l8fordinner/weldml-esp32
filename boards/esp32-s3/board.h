#pragma once

/* board.h — Generic ESP32-S3 */

#define BOARD_DUAL_CORE       1
#define BOARD_HAS_BT_CLASSIC  0
#define BOARD_HAS_BLE         1
#define BOARD_HAS_WIFI        1
#define BOARD_HAS_NATIVE_USB  1
#define BOARD_HAS_ZIGBEE      0

#define BOARD_LED_PIN         48
#define BOARD_BUTTON_PIN      0

#define BOARD_I2C_SDA_PIN     8
#define BOARD_I2C_SCL_PIN     9

#define BOARD_SPI_MOSI_PIN    11
#define BOARD_SPI_MISO_PIN    13
#define BOARD_SPI_CLK_PIN     12
#define BOARD_SPI_CS_PIN      10
