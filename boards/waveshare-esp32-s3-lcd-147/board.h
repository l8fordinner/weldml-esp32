#pragma once

/*
 * board.h — Waveshare ESP32-S3-LCD-1.47
 *
 * Chip:   ESP32-S3 QFN56 rev0.2
 * Flash:  16MB Winbond W25Q128 (Quad SPI)
 * PSRAM:  8MB Octal PSRAM
 * USB:    Native USB only (GPIO19/GPIO20) — no USB-UART bridge
 * Source: Schematic ESP32-S3-LCD-1.47 + SmrtUsbEsp hardware confirmation
 * Verified on hardware: 2026-06-19
 */

/* ── Capability flags ────────────────────────────────────────────────────── */

#define BOARD_DUAL_CORE       1
#define BOARD_HAS_BT_CLASSIC  0
#define BOARD_HAS_BLE         1
#define BOARD_HAS_WIFI        1
#define BOARD_HAS_NATIVE_USB  1   /* GPIO19/GPIO20 D-/D+ — do not reconfigure */
#define BOARD_HAS_ZIGBEE      0
#define BOARD_HAS_PSRAM       1   /* 8MB Octal PSRAM */

/* ── Boot button ──────────────────────────────────────────────────────────── */

/* GPIO0 — 10KΩ pull-up to 3.3V, active LOW. Safe to read as input at runtime.
 * Must not be driven LOW by firmware at boot. */
#define BOARD_BUTTON_PIN      0

/* ── Status LED ───────────────────────────────────────────────────────────── */

/* GPIO38 — WS2812B RGB LED, NeoPixel single-wire protocol.
 * NOT a simple GPIO toggle. Requires RMT or bit-bang NeoPixel driver.
 * Blink white during host SD write activity (SmrtUsbEsp convention). */
#define BOARD_RGB_LED_PIN     38

/* ── LCD interface ────────────────────────────────────────────────────────── */

/* Controller: ST7789V3 (possibly GC9307N on some units — verify on hardware).
 * See OPEN_QUESTIONS.md Q7. */

/* CAUTION: GPIO45 is a boot strapping pin. Must not be driven HIGH at reset.
 * Verify pull-down state before modifying LCD init sequence. */
#define BOARD_LCD_MOSI_PIN    45
#define BOARD_LCD_CLK_PIN     40
#define BOARD_LCD_CS_PIN      42
#define BOARD_LCD_DC_PIN      41   /* Data/Command select */
#define BOARD_LCD_RST_PIN     39   /* Active LOW reset */

/* GPIO48 — LCD backlight MOSFET gate, active HIGH, PWM-capable.
 * Do NOT use as a general-purpose output or LED. */
#define BOARD_LCD_BL_PIN      48

/* ── SD card SPI interface ────────────────────────────────────────────────── */

/* Hardware-confirmed via SmrtUsbEsp source + schematic cross-check.
 * SD is MSC-exclusive while USB host is active. See OPEN_QUESTIONS.md Q3. */
#define BOARD_SD_CLK_PIN      14
#define BOARD_SD_MOSI_PIN     15
#define BOARD_SD_MISO_PIN     16
#define BOARD_SD_CS_PIN       21
