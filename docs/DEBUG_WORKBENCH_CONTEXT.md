# Debug Workbench Context

## Infrastructure

**Universal Embedded Workbench**
Repo: https://github.com/SensorsIot/Universal-Embedded-Workbench

A Raspberry Pi (Pi Zero W2 or similar) configured as a networked embedded development
instrument. It is the primary remote debug and flash infrastructure for this project.

Capabilities it may provide (confirm before depending on each):

| Capability | Notes |
|------------|-------|
| Serial proxy (RFC2217) | Allows `idf.py flash` / `idf.py monitor` from WSL over WiFi |
| Remote esptool | Flash via serial proxy without physical USB connection |
| WiFi AP / STA tooling | Useful for provisioning tests in later milestones |
| GPIO-controlled boot/reset | Pi GPIO lines wired to target ESP32 BOOT and RESET |
| UDP log capture | Alternative log path when native USB is occupied by MSC |
| Automated test workflows | HTTP API; Claude Code skills available |
| OTA server | Local HTTP firmware hosting for OTA tests |
| BLE proxy | For BLE provisioning tests |

---

## Target Board: Waveshare ESP32-S3-LCD-1.47

### USB and Flash Interface

This board uses **native USB only**. There is no USB-UART bridge chip (no CH340, CP2102, FTDI).

| Property | Value |
|----------|-------|
| USB D− | GPIO19 |
| USB D+ | GPIO20 |
| Series resistors | 22 Ω on each data line |
| Serial port (WSL) | `/dev/ttyACM0` (USB CDC ACM) |
| Flash tool | `esptool.py` or `idf.py flash` |
| Monitor | `idf.py monitor` via same ACM port |

### BOOT and RESET Buttons

| Button | Label | Signal | GPIO |
|--------|-------|--------|------|
| BOOT   | Key1  | IO0 → GND (10 KΩ pull-up) | GPIO0 |
| RESET  | Key2  | EN → GND (10 KΩ pull-up) | CHIP_PU |

**Download mode entry:** Hold Key1 (BOOT), press and release Key2 (RESET), release Key1.
No auto-reset circuit exists — there is no DTR/RTS wiring from a USB-UART chip.

### Native USB Consideration During MSC Operation

When firmware is running in USB MSC mode (SmrtUsbEsp heritage), the native USB interface
is occupied by the MSC + CDC enumeration. In that state:

- The CDC log channel is available for monitoring but shares the USB bus.
- The device **cannot be reflashed** via USB without first leaving MSC mode.
- Reflashing requires either: (a) manual BOOT+RESET button sequence, or
  (b) CDC command that triggers a reboot into download mode.

If the Pi workbench is the primary flash path, this constraint affects workflow design.

---

## Prior Board Comparison

| Property | Adafruit HUZZAH32 | Waveshare ESP32-S3-LCD-1.47 |
|----------|------------------|------------------------------|
| Chip | ESP32 (D0WD-V3) | ESP32-S3R8 |
| USB interface | CP2104 USB-UART | Native USB (GPIO19/20) |
| Serial port | `/dev/ttyUSB0` | `/dev/ttyACM0` |
| BOOT button | Not standard | Key1 / GPIO0 |
| RESET button | Not standard | Key2 / EN |
| Auto-reset circuit | Via CP2104 RTS/DTR | None |
| Pi GPIO boot assist | May have been wired | Unknown — must verify |
| Flash command | `idf.py -p /dev/ttyUSB0 flash` | `idf.py -p /dev/ttyACM0 flash` |
| Monitor command | `idf.py -p /dev/ttyUSB0 monitor` | `idf.py -p /dev/ttyACM0 monitor` |

**Do not carry over HUZZAH32 assumptions** about Pi GPIO wiring, serial port paths, or
auto-reset behavior.

---

## Workbench Validation Checklist (Before Automated Flash Workflow)

These must be confirmed on the actual bench before any automated flash/test cycle:

- [ ] Waveshare board is physically connected to Pi (USB or USB hub)
- [ ] Pi sees `/dev/ttyACM0` when board is in normal run mode
- [ ] Pi sees the board in esptool download mode after manual BOOT+RESET
- [ ] `esptool.py chip_id` succeeds via Pi (confirms serial proxy path)
- [ ] Pi GPIO lines for Key1/Key2, if wired, are confirmed against Waveshare schematic
- [ ] Pi GPIO polarity and voltage levels are compatible with 3.3 V pull-up on Key1/Key2
- [ ] UDP log capture confirmed working (if native USB is occupied by MSC)

---

## Flash Workflow Options (Ranked by Complexity)

1. **Manual BOOT+RESET (lowest risk, recommended for early work)**
   - Operator holds Key1, presses Key2, releases both, runs `idf.py flash`.
   - No Pi GPIO wiring required.
   - Sufficient for development phase.

2. **Pi GPIO-assisted BOOT+RESET**
   - Pi drives Key1 and Key2 via open-drain GPIO or FET.
   - Requires wiring verification against Waveshare schematic.
   - Confirm GPIO levels and pull-up compatibility before wiring.
   - Review Universal Embedded Workbench repo for supported wiring patterns.

3. **CDC reboot command**
   - Firmware listens on CDC for a special command that reboots into download mode.
   - Requires firmware support; useful when Pi USB passthrough is available.
   - Eliminates need for Pi GPIO wiring.

4. **OTA (later milestone)**
   - No USB required for firmware updates once WiFi is enabled.
   - Out of scope for MVP.

---

## Claude Code Skills Available for Workbench

The `.claude/skills/` directory contains workbench-specific skills:

| Skill | Purpose |
|-------|---------|
| `esp-idf-handling` | Build, flash, monitor via idf.py; detects workbench vs local |
| `workbench-logging` | Serial monitor, UDP log capture, boot output, crash analysis |
| `workbench-debug` | GDB/JTAG via OpenOCD and USB JTAG (ESP32-S3 supports this) |
| `workbench-wifi` | WiFi AP/STA configuration via workbench HTTP API |
| `workbench-ble` | BLE scanning and GATT operations via Pi BLE proxy |
| `workbench-integration` | Full workbench integration for a project |
| `workbench-test-handling` | Automated test sessions with operator interaction |

ESP32-S3 native USB JTAG: the ESP32-S3 supports USB JTAG debugging via the same native USB
port (no external JTAG probe needed). See `workbench-debug` skill for setup details.
