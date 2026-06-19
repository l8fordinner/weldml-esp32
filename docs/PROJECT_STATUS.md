# Project Status

Single source of truth for cross-session continuity.
Update this at the end of each working session and commit it with the session's changes.

---

## Current State (2026-06-19)

**Phase:** Pre-implementation — hardware validation complete. No firmware code written yet.
Firmware work still blocked on Q1 and Q2 (codebase and build system strategy).

**Branch:** `main` (up to date after operating-setup and model-export commits)
**Last committed:** `2e557f8` "Add project operating and context policies"

---

## Open Questions — Resolution Log

See `docs/OPEN_QUESTIONS.md` for full context on each question.

| Q | Title | Status | Decision | Date |
|---|-------|--------|----------|------|
| Q1 | Port SmrtUsbEsp or rebase around it? | **Open** | | |
| Q2 | Native ESP-IDF or PlatformIO? | **Open** (follows Q1) | | |
| Q3 | SD ownership transition (MSC → firmware) | **Open** (follows Q6) | | |
| Q4 | Pi workbench flash path for Waveshare | **Partial** — USB + chip-id confirmed; WSL→Pi flash path unresolved (Q9) | 2026-06-19 |
| Q5 | Manual BOOT/RESET sufficient for early work? | **Resolved** — confirmed working; required (no auto-reset circuit) | 2026-06-19 |
| Q6 | Robot file-completion signal | **Open** | | |
| Q7 | LCD driver structure | **Open** (follows Q1/Q2) | | |
| Q8 | Weld file format | **Open** | | |

---

## Hardware Test Log

| Test | Board | Port | Date | Result | Notes |
|------|-------|------|------|--------|-------|
| idf.py build | HUZZAH32 (ESP32) | /dev/ttyUSB0 | (prior session) | PASS | Template build, not WeldML firmware |
| idf.py flash | HUZZAH32 (ESP32) | /dev/ttyUSB0 | (prior session) | PASS | 987 KB @ 541 kbit/s |
| Boot log, OTA table | HUZZAH32 (ESP32) | /dev/ttyUSB0 | (prior session) | PASS | IDF v5.3.2 |
| WiFi SoftAP + web UI | HUZZAH32 (ESP32) | /dev/ttyUSB0 | (prior session) | PASS | All 4 pages verified |
| MQTT connect | HUZZAH32 (ESP32) | /dev/ttyUSB0 | (prior session) | PASS | |
| OTA HTTP pull | HUZZAH32 (ESP32) | /dev/ttyUSB0 | (prior session) | PASS | ota_1 partition |
| Pi reachability | Waveshare (via Pi) | SSH 192.168.1.43 | 2026-06-19 | PASS | casey@PiEspWrkbench, key auth |
| CDC enumeration on Pi | Waveshare | /dev/ttyACM0 | 2026-06-19 | PASS | SmrtUsbEsp run mode |
| MSC enumeration on Pi | Waveshare | sda (29 GiB) | 2026-06-19 | PASS | TinyUSB Flash Storage 0.2; SD card visible |
| BOOT+RESET download mode | Waveshare | — | 2026-06-19 | PASS | Manual sequence required; no auto-reset circuit |
| chip-id (esptool) | Waveshare | /dev/ttyACM0 (Pi) | 2026-06-19 | PASS | ESP32-S3 QFN56 rev0.2, 8MB PSRAM, MAC 98:3d:ae:e4:4e:ac |
| flash-id (esptool) | Waveshare | /dev/ttyACM0 (Pi) | 2026-06-19 | PASS | 16MB Winbond W25Q128 (ef/4018), 3.3V quad SPI |
| idf.py build (esp32s3) | Waveshare (WSL build) | — | 2026-06-19 | PASS | IDF v5.3.2, 1028/1028 targets, zero errors |
| idf.py flash | Waveshare | — | — | NOT RUN | Blocked: flash size fix needed + Q9 (WSL→Pi path) |

---

## Workbench Status

| Item | Status |
|------|--------|
| Workbench Pi available | **Confirmed** — 192.168.1.43, casey@PiEspWrkbench, SSH key auth working |
| Pi sees Waveshare on /dev/ttyACM0 | **Confirmed** — CDC ACM in run mode, JTAG/serial in download mode |
| Pi sees Waveshare MSC (sda) | **Confirmed** — 29 GiB SD card via TinyUSB MSC |
| esptool on Pi | **Confirmed** — v5.2.0 at /usr/local/bin/esptool.py |
| ESP-IDF on Pi | **Not installed** — flash must run from WSL |
| WSL→Pi flash path (RFC2217 or other) | **Unverified** — see Q9 |
| Pi boot/reset GPIO wiring for Waveshare | **Unverified** — do not assume HUZZAH32 wiring applies |
| Manual BOOT+RESET sequence | **Confirmed working** — hold Key1, tap Key2, release Key1 |
| UDP log capture on Waveshare | **Not yet verified** |

---

## What Is Done

- [x] Hardware documentation extracted from schematic and datasheet (see docs/)
- [x] Verified pin table for Waveshare ESP32-S3-LCD-1.47 (schematic + SmrtUsbEsp confirmation)
- [x] Project context docs created: PRIOR_WORK_CONTEXT, MVP_REQUIREMENTS, DEBUG_WORKBENCH_CONTEXT, OPEN_QUESTIONS
- [x] Operating rules: CLAUDE.md (local), CONTEXT_POLICY.md, CODING_GUIDELINES.md, MODEL_ROUTING.md
- [x] Frozen WeldML ESP32 model exports committed (model_exports/esp32_port/)
- [x] Pi workbench confirmed reachable; SSH key auth established from WSL
- [x] Board hardware verified on Pi: chip ESP32-S3 QFN56 rev0.2, 8MB PSRAM, 16MB flash (Winbond W25Q128), 40MHz
- [x] BOOT+RESET download mode confirmed working (manual sequence required)
- [x] IDF v5.3.2 build confirmed for esp32s3 target (1028/1028, zero errors)
- [x] Q5 resolved: manual BOOT+RESET sufficient for MVP dev phase

## What Is Blocked

- [ ] **First flash** — blocked on: (1) fix sdkconfig.defaults flash size 4MB→16MB, (2) resolve Q9 (WSL→Pi flash path)
- [ ] `boards/waveshare-esp32-s3-lcd-147/` board config — blocked on Q1/Q2
- [ ] Any firmware components (LCD driver, SD init, inference engine) — blocked on Q1/Q2/Q6/Q8
- [ ] Automated workbench flash workflow — blocked on Q9 + Pi GPIO wiring (unverified)

## What Is Next

1. **Resolve Q9** — verify RFC2217 proxy on Pi (`ps aux | grep rfc2217`) or choose alternate flash path.
2. **Fix flash size** — update `sdkconfig.defaults`: `CONFIG_ESPTOOLPY_FLASHSIZE_4MB=n`, `CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`, update partition table for 16MB.
3. **First flash of Waveshare board** — after Q9 resolved and flash size fixed; requires explicit approval.
4. **Resolve Q1 and Q2** — codebase strategy and build system decision; blocks all firmware components.
5. **Create board config** `boards/waveshare-esp32-s3-lcd-147/` once Q1/Q2 resolved.

---

## Known Conflicts to Resolve Before First Build

| Issue | File | Current value | Correct value |
|-------|------|--------------|--------------|
| Flash size | `sdkconfig.defaults` | 4 MB | 16 MB (W25Q128JVSI) |
| PSRAM not enabled | `sdkconfig.defaults` | not set | CONFIG_SPIRAM=y, CONFIG_SPIRAM_MODE_OCT=y |
| Generic SPI pins | `boards/esp32-s3/board.h` | MOSI=11, CLK=12, CS=10 | LCD: MOSI=45, CLK=40, CS=42; SD: CLK=14, MOSI=15, CS=21 |
| LED pin conflict | `boards/esp32-s3/board.h` | BOARD_LED_PIN=48 | GPIO48 is LCD backlight — cannot be used as LED |
| Partition table | `partitions.csv` | 4 MB layout | Needs 16 MB layout for Waveshare board |
