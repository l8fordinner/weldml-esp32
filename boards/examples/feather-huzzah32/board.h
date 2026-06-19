#pragma once

/*
 * board.h — Adafruit Feather HUZZAH32
 *
 * ESP32-WROOM-32 module on the Adafruit Feather form factor.
 * Pinout reference: https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/pinouts
 *
 * Feather GPIO numbers map directly to ESP32 GPIO numbers.
 * Analog pins Ax are noted alongside their GPIO equivalents.
 */

/* ── Capability flags ────────────────────────────────────────────────────── */

#define BOARD_DUAL_CORE       1
#define BOARD_HAS_BT_CLASSIC  1
#define BOARD_HAS_BLE         1
#define BOARD_HAS_WIFI        1
#define BOARD_HAS_NATIVE_USB  0   /* CP2104 USB-to-serial, not native USB */
#define BOARD_HAS_ZIGBEE      0

/* ── Board-specific pins ─────────────────────────────────────────────────── */

/* LED — red LED, active HIGH */
#define BOARD_LED_PIN         13

/* Battery voltage sense — GPIO 35 = A13, 1/2 voltage divider
   ADC reading * 2 * (3.3 / 4095) gives approximate battery voltage. */
#define BOARD_VBAT_PIN        35

/* UART (USB-to-serial via CP2104) */
#define BOARD_UART_TX_PIN     1
#define BOARD_UART_RX_PIN     3

/* I2C — Feather standard */
#define BOARD_I2C_SDA_PIN     23
#define BOARD_I2C_SCL_PIN     22

/* SPI — Feather standard (VSPI) */
#define BOARD_SPI_MOSI_PIN    18
#define BOARD_SPI_MISO_PIN    19
#define BOARD_SPI_CLK_PIN     5
#define BOARD_SPI_CS_PIN      33

/* Feather analog header pins → GPIO equivalents */
#define BOARD_A0_PIN          26   /* DAC1 capable */
#define BOARD_A1_PIN          25   /* DAC2 capable */
#define BOARD_A2_PIN          34   /* input only */
#define BOARD_A3_PIN          39   /* input only (SVN) */
#define BOARD_A4_PIN          36   /* input only (SVP) */
#define BOARD_A5_PIN          4

/* Feather digital header pins */
#define BOARD_D4_PIN          4    /* same as A5 */
#define BOARD_D5_PIN          5    /* SPI CLK */
#define BOARD_D6_PIN          6    /* NOTE: GPIO 6-11 connected to flash — avoid */
#define BOARD_D7_PIN          21
#define BOARD_D8_PIN          13   /* LED */
#define BOARD_D9_PIN          27
#define BOARD_D10_PIN         33
#define BOARD_D11_PIN         15
#define BOARD_D12_PIN         32
#define BOARD_D13_PIN         14
#define BOARD_D14_PIN         32   /* Feather RX/TX label varies by revision */
#define BOARD_D15_PIN         14
#define BOARD_D16_PIN         16   /* RX2 */
#define BOARD_D17_PIN         17   /* TX2 */
#define BOARD_D18_PIN         18   /* SPI MOSI */
#define BOARD_D19_PIN         19   /* SPI MISO */
#define BOARD_D20_PIN         20   /* not broken out on all revisions */
#define BOARD_D21_PIN         21   /* I2C SDA */
