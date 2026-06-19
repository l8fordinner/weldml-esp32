# Prior Work Context

## SmrtUsbEsp

**Repo:** https://github.com/l8fordinner/SmrtUsbEsp

Firmware for the Waveshare ESP32-S3-LCD-1.47 board, implementing a USB-A/USB dongle that
exposes the onboard microSD card to a host (PC or robot) as USB Mass Storage (MSC) via TinyUSB,
while streaming diagnostic logs over a second TinyUSB CDC interface.

### What It Does

- Enumerates as a USB MSC device over native USB (GPIO19/GPIO20).
- Host mounts the SD card as a removable drive and writes weld data files to it.
- Streams firmware logs to the host over USB CDC (second interface).
- White status: WS2812B RGB LED on GPIO38 blinks during host write activity.
- The SD card is **not locally mounted** while the host MSC session is active. MSC callbacks
  keep the medium exclusive to the host. Firmware must not access SD during this window.

### Build System

PlatformIO + ESP-IDF (platformio.ini drives idf.py under the hood).

### Verified SD SPI Pin Assignments

These were used in SmrtUsbEsp and match the Waveshare schematic exactly:

| Signal | GPIO |
|--------|------|
| CLK    | 14   |
| MOSI   | 15   |
| MISO   | 16   |
| CS     | 21   |

These pins are hardware-confirmed from the schematic (see schematic_diagram.pdf) and should be
treated as fixed for any fork or port of this firmware.

### SD Ownership Model

SmrtUsbEsp establishes an important architectural constraint: SD access is **exclusive** — either
the USB host owns it (MSC active) or the firmware owns it (local mount). There is no concurrent
access. Any WeldML firmware that reads SD files must implement a safe handoff strategy.

A safe access transition requires one of:
- USB host ejecting the MSC volume, then firmware re-mounting.
- A firmware-side signal (CDC command, GPIO trigger, quiet-period detection).
- Explicit ownership arbitration in the MSC callbacks.

### Relevance to WeldML Port

SmrtUsbEsp is the immediate predecessor firmware for this board and hardware configuration.
The WeldML port either builds on top of it or replaces it. The SD pin assignments, TinyUSB
component usage, and SD ownership model are all directly relevant design inputs.

---

## Universal Embedded Workbench (Pi Debug Infrastructure)

**Repo:** https://github.com/SensorsIot/Universal-Embedded-Workbench

A Raspberry Pi / Pi Zero W2 setup used as remote debug and test infrastructure for ESP32
development. It may provide:

- Serial proxy (RFC2217) for remote `idf.py flash` and `idf.py monitor`
- Remote flashing via esptool over the serial proxy
- A WiFi AP or station for provisioning tests
- GPIO-controlled boot/reset lines to the ESP32
- UDP log capture for boards with native USB occupied
- Automated test workflows via HTTP API (Claude Code skills available)

### Prior Board: Adafruit HUZZAH32 (ESP32 Feather)

The workbench was previously used with a HUZZAH32 / ESP32 classic. That board uses a
USB-UART chip (CP2104) for flashing and serial, and requires RTS/DTR signaling for
auto-reset into download mode. It does **not** have dedicated BOOT/RESET buttons in the
standard two-button layout.

This caused friction: boot/reset GPIO wiring on the Pi had to compensate for the missing
hardware buttons, and the auto-reset circuit behavior differed from ESP32-S3 native USB.

### Current Target: Waveshare ESP32-S3-LCD-1.47

The Waveshare board has a very different hardware interface:

- **Native USB only** — no USB-UART bridge chip.
- **Two dedicated hardware buttons:** BOOT (Key1, GPIO0) and RESET (Key2, EN/CHIP_PU).
- Serial console appears as `/dev/ttyACM0` (USB CDC ACM), not `/dev/ttyUSB0`.
- Flashing requires manual or GPIO-assisted BOOT+RESET sequence to enter download mode.

**Do not assume HUZZAH32 wiring applies.** Do not assume Pi GPIO boot/reset connections are
wired or required until the Universal Embedded Workbench wiring and the Waveshare schematic
have been reviewed together. The two-button layout is new to this target and should be validated
on the bench before any automated flash workflow is depended on.

### Workbench Validation Needed

Before relying on the workbench for automated flash/test cycles on the Waveshare board:

1. Confirm which workbench GPIO lines (if any) connect to the Waveshare board's Key1/Key2.
2. Confirm the esptool port (`/dev/ttyACM0` or RFC2217 proxy path).
3. Confirm native USB is visible to the Pi (or requires USB hub passthrough from host).
4. Verify a manual BOOT+RESET sequence successfully enters download mode.

For early development, manual button presses are a valid and lower-risk starting point.
