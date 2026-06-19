# Project Status

Single source of truth for cross-session continuity.
Update this at the end of each working session and commit it with the session's changes.

---

## Current State (2026-06-19)

**Phase:** Pre-implementation — hardware validation and workbench path verification complete.
No firmware code written. First smoke-test flash is now unblocked (awaiting explicit approval).
Firmware work still blocked on Q1 and Q2 (codebase and build system strategy).

**Branch:** `main`
**Last committed:** `791f348` "Reclassify 4MB flash size as non-blocking for smoke-test flash"

**Board state at session end:** Board may be in download mode (USB JTAG/serial debug unit).
Press RST (Key2) to return to SmrtUsbEsp run mode before next session.

---

## Session Handoff — 2026-06-19

**Goal this session:** Verify WSL-to-Pi flash/monitor path (Q9).

**Completed:**
- Confirmed `rfc2217-portal` service running on Pi since boot (systemd, auto-starts)
- `plain_rfc2217_server.py -p 4003 /dev/ttyACM0` auto-spawned when board connected
- Port 4003 confirmed listening on 0.0.0.0 (accessible from WSL)
- WSL→Pi RFC2217 connection test: `esptool.py --port rfc2217://192.168.1.43:4003 chip_id` succeeded
- OpenOCD also running on Pi: GDB port 3335, telnet port 4446 (`esp32s3-builtin.cfg`)

**Q9 result: Resolved.** RFC2217 on port 4003 is the confirmed flash/monitor path from WSL.

**First smoke-test flash: unblocked.** Only remaining gate is explicit user approval.

**Exact commands for first flash (do not run until approved):**
```bash
# Step 1 — build (WSL)
. ~/esp/esp-idf/export.sh && cd /mnt/j/ReposWSL/weldml-esp32 && idf.py build

# Step 2 — enter download mode (physical: hold Key1, tap Key2, release Key1)

# Step 3 — flash (WSL via RFC2217)
. ~/esp/esp-idf/export.sh && idf.py -p rfc2217://192.168.1.43:4003 flash

# Step 4 — monitor (WSL via RFC2217, board in run mode)
. ~/esp/esp-idf/export.sh && idf.py -p rfc2217://192.168.1.43:4003 monitor
# Exit monitor: Ctrl+]
```

**Files changed this session (not yet committed):**
- `docs/PROJECT_STATUS.md` — this file (handoff + Q9 result)
- No source files modified

**Verification status:**
- RFC2217 connection: verified ✓
- Flash: NOT RUN — awaiting approval
- Monitor: NOT RUN — awaiting approval
- OpenOCD/JTAG: present but not tested from WSL

**Next action:** Say "approve first flash" to proceed, or resolve Q1/Q2 for firmware work.

---

## Open Questions — Resolution Log

See `docs/OPEN_QUESTIONS.md` for full context on each question.

| Q | Title | Status | Decision | Date |
|---|-------|--------|----------|------|
| Q1 | Port SmrtUsbEsp or rebase around it? | **Open** | | |
| Q2 | Native ESP-IDF or PlatformIO? | **Open** (follows Q1) | | |
| Q3 | SD ownership transition (MSC → firmware) | **Open** (follows Q6) | | |
| Q4 | Pi workbench flash path for Waveshare | **Resolved** — RFC2217 port 4003 confirmed; see Q9 | 2026-06-19 |
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
| idf.py flash | Waveshare | — | — | NOT RUN | Q9 resolved; awaiting explicit approval |

---

## Workbench Status

| Item | Status |
|------|--------|
| Workbench Pi available | **Confirmed** — 192.168.1.43, casey@PiEspWrkbench, SSH key auth working |
| Pi sees Waveshare on /dev/ttyACM0 | **Confirmed** — CDC ACM in run mode, JTAG/serial in download mode |
| Pi sees Waveshare MSC (sda) | **Confirmed** — 29 GiB SD card via TinyUSB MSC |
| esptool on Pi | **Confirmed** — v5.2.0 at /usr/local/bin/esptool.py |
| ESP-IDF on Pi | **Not installed** — flash must run from WSL |
| WSL→Pi flash path (RFC2217 or other) | **Confirmed** — RFC2217 port 4003 (`plain_rfc2217_server.py`); `esptool` chip_id tested successfully from WSL |
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
- [x] Q9 resolved: RFC2217 port 4003 confirmed as WSL→Pi flash/monitor path
- [x] OpenOCD confirmed running on Pi (GDB 3335, telnet 4446, esp32s3-builtin.cfg)

## What Is Blocked

- [ ] **First flash** — awaiting explicit approval only; all technical blockers cleared
- [ ] `boards/waveshare-esp32-s3-lcd-147/` board config — blocked on Q1/Q2
- [ ] Any firmware components (LCD driver, SD init, inference engine) — blocked on Q1/Q2/Q6/Q8
- [ ] Automated workbench flash workflow — Pi GPIO wiring for Key1/Key2 still unverified

## What Is Next

1. **First flash of Waveshare board** — say "approve first flash"; use `idf.py -p rfc2217://192.168.1.43:4003 flash` after BOOT+RESET.
2. **Resolve Q1 and Q2** — codebase strategy and build system decision; blocks all firmware components.
3. **Create board config** `boards/waveshare-esp32-s3-lcd-147/` once Q1/Q2 resolved — include `CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y` to fix flash size correctly.

---

## Known Conflicts to Resolve Before First Build

| Issue | File | Current value | Correct value | Blocks first smoke-test flash? |
|-------|------|--------------|--------------|-------------------------------|
| Flash size declared as 4MB | root `sdkconfig.defaults` | 4 MB | 16 MB in `boards/waveshare-esp32-s3-lcd-147/sdkconfig.defaults` | **No** — partitions.csv fits within 4MB; upper 12MB unused but safe |
| PSRAM not enabled | `sdkconfig.defaults` | not set | CONFIG_SPIRAM=y, CONFIG_SPIRAM_MODE_OCT=y | No — template firmware does not use PSRAM |
| Generic SPI pins | `boards/esp32-s3/board.h` | MOSI=11, CLK=12, CS=10 | LCD: MOSI=45, CLK=40, CS=42; SD: CLK=14, MOSI=15, CS=21 | Yes — for WeldML firmware; not for template smoke test |
| LED pin conflict | `boards/esp32-s3/board.h` | BOARD_LED_PIN=48 | GPIO48 is LCD backlight — cannot be used as LED | Yes — for WeldML firmware; not for template smoke test |
| Partition table comment | `partitions.csv` | "Total flash assumed: 4 MB" | No functional change needed; layout is address-based and safe on 16MB | No |

**Fix path for flash size:** Create `boards/waveshare-esp32-s3-lcd-147/sdkconfig.defaults` with
`CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`. Do **not** change root `sdkconfig.defaults` or
`boards/esp32-s3/sdkconfig.defaults`. Required before OTA layout expansion, SPIFFS
growth above 4MB, or production WeldML firmware deployment.
