#pragma once

/*
 * board.h — Generic ESP32 board definitions.
 *
 * This file is included by components that need board-specific pin assignments
 * or capability flags.  Override in a product fork by providing a board-specific
 * copy of this file earlier in the include path.
 *
 * Pin assignments below are illustrative defaults — adjust for your hardware.
 */

/* ── Capability flags ────────────────────────────────────────────────────── */
/* These are also exposed as Kconfig symbols (see sdkconfig.defaults).
   Use CONFIG_BOARD_* in C code; use these macros only in board.h itself. */

#define BOARD_DUAL_CORE       1
#define BOARD_HAS_BT_CLASSIC  1
#define BOARD_HAS_BLE         1
#define BOARD_HAS_WIFI        1
#define BOARD_HAS_NATIVE_USB  0
#define BOARD_HAS_ZIGBEE      0

/* ── Generic pin assignments ─────────────────────────────────────────────── */

#define BOARD_LED_PIN         2   /* Built-in LED on most ESP32 dev boards */
#define BOARD_BUTTON_PIN      0   /* BOOT button */

/* UART */
#define BOARD_UART_TX_PIN     1
#define BOARD_UART_RX_PIN     3

/* I2C (default) */
#define BOARD_I2C_SDA_PIN     21
#define BOARD_I2C_SCL_PIN     22

/* SPI (default VSPI) */
#define BOARD_SPI_MOSI_PIN    23
#define BOARD_SPI_MISO_PIN    19
#define BOARD_SPI_CLK_PIN     18
#define BOARD_SPI_CS_PIN      5
