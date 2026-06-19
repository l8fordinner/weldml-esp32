# Project Status

Single source of truth for cross-session continuity.
Update this at the end of each working session and commit it with the session's changes.

---

## Current State (2026-06-19)

**Phase:** Stage 4 IN PROGRESS. Build PASSED (1066/1066, 357 KB, zero errors).
Flash blocked — workbench Pi (192.168.1.43) went offline mid-session.
Next action: restore workbench connectivity, then flash.

**Branch:** `main`
**Last committed:** f0e66c2 (Stage 4 code + CMake; build confirmed passing this session)

**Board state:** SLOT3. Firmware from Stage 3 (green LCD) still running on hardware.
OpenOCD stopped (via /api/debug/stop) before flash attempt. Pi offline — board state unknown.

---

## Session Handoff — 2026-06-19 (Stage 4 implementation — build not yet run)

**Goal:** Stage 4 — USB MSC + SD SPI. Expose SD card as USB mass storage device.

**Files changed this session:**
- `components/lcd_st7789/lcd_st7789.c` — added `esp_lcd_panel_mirror(true, true)` after
  `esp_lcd_panel_init()` to fix 180° MADCTL rotation (deferred orientation fix from Stage 3)
- `components/usb_msc_sd/` (NEW component):
  - `CMakeLists.txt` — REQUIRES: driver, sdmmc, esp_tinyusb
  - `usb_msc_sd.h` — `usb_msc_sd_config_t` (miso/mosi/clk/cs pins) + `usb_msc_sd_init()`
  - `usb_msc_sd.c` — SPI3_HOST bus init, SDSPI host init, sdmmc_card_init, TinyUSB MSC+CDC
  - `idf_component.yml` — `espressif/esp_tinyusb: ^1.4.2`
- `main/main.c` — Stage 4: LCD (white→green/red) + `usb_msc_sd_init()` with board SD pins
- `main/CMakeLists.txt` — added `usb_msc_sd` to REQUIRES
- `main/idf_component.yml` (NEW) — `espressif/esp_tinyusb: ^1.4.2` (needed by CM)
- `idf_component.yml` (root) — added `espressif/esp_tinyusb: ^1.4.2`
- `boards/waveshare-esp32-s3-lcd-147/sdkconfig.defaults` — added
  `CONFIG_TINYUSB_CDC_ENABLED=y` and `CONFIG_TINYUSB_MSC_ENABLED=y`
- `docs/OPEN_QUESTIONS.md` — closed Q1, Q2, Q7
- `dependencies.lock` (NEW) — component manager lock file; commit this

**Q1/Q2/Q7 decisions (now closed):**
- Q1: Port SmrtUsbEsp code as new ESP-IDF components (Option A). Done: `components/usb_msc_sd/`
- Q2: Native ESP-IDF. In use since Stage 1.
- Q7: `esp_lcd` + ST7789 driver. Implemented in Stage 3.

**Build/flash/monitor status:**
- `idf.py set-target esp32s3` — DONE (CMake configured, target correct)
- `espressif/esp_tinyusb 1.7.6~2` + `espressif/tinyusb 0.19.0~3` — downloaded to
  `managed_components/` (gitignored; `dependencies.lock` committed instead)
- `BOARD=waveshare-esp32-s3-lcd-147 idf.py build` — **PASS** (1066/1066, 357 KB, zero errors, 2026-06-19)
- Flash — **BLOCKED** this session: workbench Pi went offline. Port 4003 RFC2217 returned
  "timeout while waiting for option 'purge'" then Pi became unreachable. Not a firmware issue.
- `POST /api/flash` — confirmed 404 on this portal (same as Q9 resolution; use RFC2217 fallback)

**Key finding — TinyUSB in ESP-IDF 5.3:**
- `esp_tinyusb` is NOT in core ESP-IDF 5.3; must use IDF Component Manager
- Manifest must be in COMPONENT directories (`main/idf_component.yml`,
  `components/usb_msc_sd/idf_component.yml`) — project root alone is insufficient
- `idf.py set-target esp32s3` needed after deleting `sdkconfig` (target is lost)
- `CONFIG_ESP_CONSOLE_USB_CDC=y` is ROM CDC driver, incompatible with TinyUSB — do NOT use
- Keep `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` for early boot logs; after `tinyusb_driver_install()`,
  USB PHY switches to OTG and JTAG logs stop. This is expected behavior.

**Monitoring after Stage 4 flash (important — new procedure):**
1. Capture boot log via `/api/serial/reset` BEFORE TinyUSB installs — SD card probe
   info (type, size) appears in this early log via USB Serial JTAG
2. After TinyUSB installs, USB Serial JTAG stops; verify USB MSC via Pi:
   `ssh casey@192.168.1.43 "dmesg | tail -20"` — should show USB MSC enumeration
   `ssh casey@192.168.1.43 "lsusb -t"` — should show CDC + mass storage
3. LCD shows result: green = success, red = SD card probe failed

**Hardware artifact notes (from Stage 3, carry forward):**
- Black semicircle at physical top-right = fixed hardware/panel artifact, NOT software
- Display content 180° rotated relative to USB-at-top physical orientation (now fixed in
  `lcd_st7789.c` via `mirror(true, true)`)

**Next session instructions:**
1. Read `docs/PROJECT_STATUS.md` only.
2. **Build already passed** — skip build step.
3. Verify workbench is back: `curl -s http://192.168.1.43:8080/api/info`
4. Check SLOT3 state: `curl -s http://192.168.1.43:8080/api/devices | python3 -m json.tool`
5. If OpenOCD is running, stop it: `curl -X POST http://192.168.1.43:8080/api/debug/stop -H 'Content-Type: application/json' -d '{"slot": "SLOT3"}'`
6. Flash via RFC2217 (POST /api/flash is 404 on this portal):
   ```
   cd /mnt/j/ReposWSL/weldml-esp32 && source ~/esp/esp-idf/export.sh && idf.py -p rfc2217://192.168.1.43:4003 flash
   ```
7. Capture boot log via `/api/serial/reset` (while OpenOCD is still stopped — DTR/RTS path):
   ```
   curl -X POST http://192.168.1.43:8080/api/serial/reset -H 'Content-Type: application/json' -d '{"slot": "SLOT3"}' | python3 -m json.tool
   ```
8. Check Pi dmesg/lsusb for USB MSC enumeration:
   ```
   ssh casey@192.168.1.43 "dmesg | tail -20"
   ssh casey@192.168.1.43 "lsusb -t"
   ```
9. Confirm LCD shows green (or red if SD card probe failed).
10. Update PROJECT_STATUS.md hardware test log, commit, push.
11. Stop on failure (do not start parser/model work, do not inspect .env).

---

## Session Handoff — 2026-06-19 (Stage 3 complete — visual confirmation)

**Goal:** Resolve gap calibration open issue from previous session; get user visual confirmation.

**Completed this session:**
- Confirmed via ESP-IDF source (`esp_lcd_panel_st7789.c`) that `esp_lcd_panel_mirror()`
  only sends a MADCTL update and does NOT change the CASET/RASET window. For a solid
  fill, mirror_x/mirror_y have no effect on which physical pixels are addressed.
  The previous session's gap calibration candidates 1 and 3 (mirror_x with same gap)
  were therefore not actionable for diagnosing a solid-fill black region.
- Tested `x_gap=0, y_gap=0`: black region unchanged from `x_gap=34` — confirmed the
  black is NOT from a gap/offset error.
- Added `lcd_st7789_fill_rect()` to the driver and ran a 4-quadrant color test.
- Reverted `main.c` to the original white→green pattern. x_gap=34 restored.
- Added extended color defines to `lcd_st7789.h` (BLUE, YELLOW, CYAN, MAGENTA).

**Visual confirmation results (user-reported, 2026-06-19):**

_Quadrant test (firmware sends TL=red, TR=blue, BL=cyan, BR=yellow):_
- User observed: TL=yellow, TR=cyan, BL=blue, BR=red
- This is an exact 180° rotation of the programmed layout.

_Black semicircle artifact:_
- A curved black region (quarter-circle arc, radius ≈ 1/3 of width, 10% of height)
  is present in the physical top-right corner when USB faces up.
- The arc curves inward toward the screen center (concave toward corner).
- The artifact appears identically on: solid white, solid green, and the quadrant test.
- Changing x_gap (34→0) had no effect on the artifact.
- **Conclusion: fixed hardware/panel artifact** — likely LCD glass defect, pressure
  damage, backlight masking, or damaged display area. Not a software issue.
  Not a blocker for Stage 4 firmware bring-up unless user chooses to replace the display.

**Stage 3 success criteria — all met for firmware bring-up:**
- [x] LCD driver builds (zero errors, binary ~248 KB)
- [x] Flash succeeds — bootloader, partition table, firmware SHA-verified (RFC2217)
- [x] Boot log clean — 16MB flash, 8MB PSRAM OK, white@~1s, green@~3s, no panic
- [x] Screen turns on — backlight active, color changes confirmed by user
- [x] Full-screen fill confirmed working (solid white, solid green displayed correctly)
- [x] Quadrant test: four distinct colors visible in correct relative positions
- [~] Hardware artifact: fixed black semicircle at physical top-right — hardware defect,
     not a software regression; documented and accepted for firmware bring-up

**Two open items for the next LCD session (NOT Stage 4 blockers):**
1. **MADCTL orientation**: Content is 180° rotated relative to expected with USB at top
   and MADCTL=0x00. Fix: add `esp_lcd_panel_mirror(s_panel, true, true)` after
   `esp_lcd_panel_init()` in `lcd_st7789_init()`. Confirmed needed before drawing
   any directional UI content.
2. **Black semicircle**: If the display is replaced or a second unit is available,
   test to confirm whether the artifact is unit-specific or panel-design characteristic.

**Exact next action for a fresh session:**
Stage 4 — USB MSC + SD SPI. Port TinyUSB MSC + CDC + SD SPI from SmrtUsbEsp.
Close Q1 and Q2 in OPEN_QUESTIONS.md before writing Stage 4 code.
Do not touch LCD driver orientation until Stage 4 is complete.

---

## Session Handoff — 2026-06-19 (Stage 2 board config)

**Goal:** Create `boards/waveshare-esp32-s3-lcd-147/` board config.

**Completed:**
- `boards/waveshare-esp32-s3-lcd-147/board.h` — all hardware-confirmed GPIO defines:
  LCD SPI (MOSI=45, CLK=40, CS=42, DC=41, RST=39), LCD backlight (GPIO48, MOSFET gate),
  SD SPI (CLK=14, MOSI=15, MISO=16, CS=21), WS2812B RGB LED (GPIO38), BOOT button (GPIO0).
  GPIO48 and GPIO38 named correctly to prevent misuse as generic GPIO.
- `boards/waveshare-esp32-s3-lcd-147/sdkconfig.defaults` — 16MB flash (overrides root 4MB),
  8MB Octal PSRAM (`CONFIG_SPIRAM=y`, `CONFIG_SPIRAM_MODE_OCT=y`).
- Build verified: `BOARD=waveshare-esp32-s3-lcd-147 idf.py build` → 1023/1023 targets, zero errors.
  Generated sdkconfig confirms `CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`, `CONFIG_SPIRAM_MODE_OCT=y`.
  Flash command shows `--flash_size 16MB`.
- Committed `907570d`, pushed to origin/main.

**Success criteria — all met:**
- [x] `boards/waveshare-esp32-s3-lcd-147/board.h` created with all confirmed GPIO defines
- [x] `boards/waveshare-esp32-s3-lcd-147/sdkconfig.defaults` created with 16MB flash + PSRAM
- [x] `BOARD=waveshare-esp32-s3-lcd-147 idf.py build` passes (1023/1023, zero errors)
- [x] Generated sdkconfig confirms 16MB flash and Octal PSRAM
- [x] Committed and pushed

**Next action:** Resolve Q1 (architecture decision: port SmrtUsbEsp code vs. other options)
to unblock Stage 3 firmware components (TinyUSB MSC, SD init, LCD driver).

---

## Session Handoff — 2026-06-19 (RFC2217 monitor/reset fix)

**Goal:** Resolve Stage 1 monitor blocker — establish repeatable flash → reset/boot → log workflow.

**Completed:**
- Root-caused RFC2217 monitoring issue: when OpenOCD runs, `POST /api/serial/reset` uses JTAG (`reset run`), not DTR/RTS. JTAG path returns no boot log.
- Fix: stop OpenOCD → `POST /api/serial/reset` uses DTR/RTS path → captures full boot log → restart OpenOCD.
- Verified: 76-line boot log captured in single API call: IDF v5.3.2, no panic, softAP up, "Startup complete".
- Confirmed: `plain_rfc2217_server.py` already sets DTR=False/RTS=False on startup and after client disconnect — it does NOT hold DTR=1/RTS=1. Prior note in NOTES.md was inaccurate; corrected.
- Confirmed: `idf.py monitor --no-reset` is the safe interactive monitoring command (prevents DTR=True assertion via RFC2217).
- Updated NOTES.md with full root cause analysis and verified workflow commands.
- Updated PROJECT_STATUS.md flash command block with Step 4a (boot log) and Step 4b (interactive monitor).

**Success criteria — all met:**
- [x] Repeatable flash → reset/boot → monitor sequence established
- [x] Boot log captured from flashed firmware (76 lines, verified 2026-06-19)
- [x] PROJECT_STATUS.md updated
- [x] NOTES.md corrected

**Next action:** Stage 2 — Create `boards/waveshare-esp32-s3-lcd-147/` board config. Say "proceed Stage 2" to start.

---

## Session Handoff — 2026-06-19 (Stage 1 flash attempt)

**Goal:** Stage 1 smoke-test flash. Template firmware flashed and SHA verified.
Boot log blocked by workbench monitoring issue (see NOTES.md and below).

**Completed this session:**
- 7-stage implementation plan written to `~/.claude/plans/enter-planning-mode-for-rustling-kite.md`
- Stage 5 clarified: SCSI quiet-period detection (not sentinel file); Kawasaki marker file is upgrade path
- `idf.py build` passed (BOARD unset, 4MB flash sdkconfig, 1028/1028 targets)
- Template firmware flashed via `idf.py -p rfc2217://192.168.1.43:4003 flash` — ALL 5 binaries SHA verified
- Flash path discovery: `POST /api/flash` does NOT exist on this portal; RFC2217 is the correct path
- GPIO18 (BOOT) released (portal GPIO API); confirmed Pi GPIO18 = input/value=1
- New workbench finding: when OpenOCD runs, `POST /api/serial/reset` uses JTAG (no boot log). DTR/RTS path (with boot log) requires stopping OpenOCD first. See NOTES.md RFC2217 note.

**Stage 1 complete — all success criteria met:**
- [x] Flash binary verified (SHA hash, all 5 files, esptool v4.11.0)
- [x] Boot log captured: IDF v5.3.2, no panic, no crash loop, "Startup complete"
- [x] No erase_flash used
- [x] Commit and push
- [x] SESSION_HANDOFF.md updated

## Session Handoff — 2026-06-19 (workbench discovery)

**Goal this session:** Audit existing skills; discover full extent of workbench capabilities.

**Completed:**
- Confirmed full workbench HTTP portal running at `http://192.168.1.43:8080`
- `/api/devices` confirms Waveshare on SLOT3: state=idle, devnode=/dev/ttyACM0, url=rfc2217://192.168.1.43:4003
- Pi GPIO wiring confirmed: gpio_boot=18 (→ Key1/BOOT/GPIO0), gpio_en=17 (→ Key2/EN/RST)
- Automated download mode entry via portal GPIO API — no manual BOOT+RESET required for flash
- OpenOCD auto-started on SLOT3: debugging=true, debug_chip=esp32s3, gdb_port=3335, telnet=4446
- `esp-idf-handling` skill covers the full flash workflow; no new skill needed
- Q4 fully resolved; Q9 fully resolved

**First smoke-test flash: unblocked.** Awaiting explicit approval only.

**Exact commands for first flash (do not run until approved):**
```bash
# Step 1 — add hostname alias in WSL (one-time)
echo "192.168.1.43 workbench.local" | sudo tee -a /etc/hosts

# Step 2 — build (WSL)
. ~/esp/esp-idf/export.sh && cd /mnt/j/ReposWSL/weldml-esp32 && idf.py build

# Step 3 — flash via portal (Pi-side esptool; portal enters download mode via GPIO automatically)
cd build && curl -s -X POST http://workbench.local:8080/api/flash \
  -F slot=SLOT3 -F chip=esp32s3 -F baud=921600 \
  -F flash_args=@flash_args \
  -F bootloader.bin=@bootloader/bootloader.bin \
  -F partition-table.bin=@partition_table/partition-table.bin \
  -F ota_data_initial.bin=@ota_data_initial.bin \
  -F firmware.bin=@weldml_esp32.bin \
  | jq .

# Step 4a — capture boot log (stop OpenOCD so portal uses DTR/RTS path, not JTAG)
curl -X POST http://192.168.1.43:8080/api/debug/stop -H 'Content-Type: application/json' -d '{"slot": "SLOT3"}'
curl -X POST http://192.168.1.43:8080/api/serial/reset -H 'Content-Type: application/json' -d '{"slot": "SLOT3"}' | python3 -m json.tool
curl -X POST http://192.168.1.43:8080/api/debug/start -H 'Content-Type: application/json' -d '{"slot": "SLOT3"}'
# → Full boot log in JSON "output" array; OpenOCD resumed for GDB after

# Step 4b — interactive monitor (--no-reset prevents DTR=True assertion via RFC2217)
. ~/esp/esp-idf/export.sh && idf.py -p rfc2217://192.168.1.43:4003 monitor --no-reset
# Exit monitor: Ctrl+]

# Fallback flash (if portal API unavailable): manual BOOT+RESET then:
# . ~/esp/esp-idf/export.sh && idf.py -p rfc2217://192.168.1.43:4003 flash
```

**Verification status:**
- Portal API (`/api/flash`): not yet tested with full flash — command confirmed from skill docs
- RFC2217 connection: verified ✓ (chip_id tested)
- Flash: PASS — `idf.py -p rfc2217://192.168.1.43:4003 flash`, all 5 binaries SHA verified
- Boot log: PASS — IDF v5.3.2, no panic, softAP up, "Startup complete"
- OpenOCD/JTAG: auto-started, confirmed in `/api/devices`; GDB not yet connected from WSL

**Next action:** Say "approve first flash" to proceed, or resolve Q1/Q2 for firmware work.

---

## Open Questions — Resolution Log

See `docs/OPEN_QUESTIONS.md` for full context on each question.

| Q | Title | Status | Decision | Date |
|---|-------|--------|----------|------|
| Q1 | Port SmrtUsbEsp or rebase around it? | **Resolved** | Port as new components; no fork; `components/usb_msc_sd/` | 2026-06-19 |
| Q2 | Native ESP-IDF or PlatformIO? | **Resolved** (follows Q1) | Native ESP-IDF; in use since Stage 1 | 2026-06-19 |
| Q3 | SD ownership transition (MSC → firmware) | **Open** (follows Q6) | | |
| Q4 | Pi workbench flash path for Waveshare | **Resolved** — HTTP portal + GPIO automation confirmed; `POST /api/flash` is preferred path | 2026-06-19 |
| Q5 | Manual BOOT/RESET sufficient for early work? | **Resolved** — manual confirmed; automated GPIO path also available (gpio_boot=18, gpio_en=17) | 2026-06-19 |
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
| idf.py flash (template) | Waveshare | rfc2217://192.168.1.43:4003 | 2026-06-19 | PASS | All 5 binaries SHA verified; esptool v4.11.0 |
| Boot log (template) | Waveshare | /dev/ttyACM1 (passive read) | 2026-06-19 | PASS | IDF v5.3.2, no panic, softAP up, "Startup complete" |
| LCD white→green (Stage 3) | Waveshare | rfc2217://192.168.1.43:4003 | 2026-06-19 | PASS | SPI LCD driver; white@1s, green@3s; no panic; 248 KB binary |
| LCD 4-quadrant pattern | Waveshare | rfc2217://192.168.1.43:4003 | 2026-06-19 | PASS | TL=yellow, TR=cyan, BL=blue, BR=red observed (180° rotated vs. programmed) |
| LCD black semicircle | Waveshare | — | 2026-06-19 | HW DEFECT | Fixed curved artifact top-right; present on all fills; not a software issue |
| idf.py build (Stage 4) | Waveshare (WSL build) | — | 2026-06-19 | PASS | 1066/1066, 357 KB, zero errors; usb_msc_sd + esp_tinyusb 1.7.6 linked |

---

## Workbench Status

| Item | Status |
|------|--------|
| Workbench Pi available | **Confirmed** — 192.168.1.43, casey@PiEspWrkbench, SSH key auth working |
| Pi workbench HTTP portal | **Confirmed** — `http://192.168.1.43:8080`; `slots_configured=3`; full REST API available |
| Waveshare assigned to SLOT3 | **Confirmed** — state=idle, devnode=/dev/ttyACM0, url=rfc2217://192.168.1.43:4003 |
| Pi sees Waveshare on /dev/ttyACM0 | **Confirmed** — CDC ACM in run mode, JTAG/serial in download mode |
| Pi sees Waveshare MSC (sda) | **Confirmed** — 29 GiB SD card via TinyUSB MSC |
| esptool on Pi | **Confirmed** — v5.2.0; invoked via `POST /api/flash` (portal handles lifecycle) |
| ESP-IDF on Pi | **Not installed** — build runs in WSL; Pi-side flash via portal API |
| WSL→Pi flash path | **Confirmed** — `POST /api/flash` preferred; fallback `idf.py -p rfc2217://192.168.1.43:4003 flash` |
| Pi GPIO wiring for Waveshare BOOT+RESET | **Confirmed** — gpio_boot=18 (→ Key1/GPIO0), gpio_en=17 (→ Key2/EN); automated download mode works |
| Manual BOOT+RESET sequence | **Confirmed working** — hold Key1, tap Key2, release Key1; not required when using portal API |
| OpenOCD on Pi (SLOT3) | **Confirmed** — auto-started; debug_chip=esp32s3, gdb_port=3335, telnet=4446 |
| Hostname alias in WSL | **Not yet set** — add `192.168.1.43 workbench.local` to `/etc/hosts` before first flash |
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
- [x] Q5 resolved: manual BOOT+RESET sufficient; automated GPIO path also confirmed
- [x] Q9 resolved: `POST /api/flash` 404 on this portal; RFC2217 is the confirmed flash path
- [x] Q4 resolved: Pi HTTP portal confirmed; GPIO wiring confirmed (gpio_boot=18, gpio_en=17)
- [x] OpenOCD auto-started on SLOT3 (GDB 3335, telnet 4446, esp32s3-builtin.cfg)
- [x] `esp-idf-handling` skill covers full flash workflow — no new skill needed
- [x] **Stage 1 complete** — template firmware flashed and boot-verified on Waveshare ESP32-S3
- [x] **Stage 2 complete** — `boards/waveshare-esp32-s3-lcd-147/` board config added, build verified
- [x] **Stage 3 complete** — LCD driver verified on hardware; white→green confirmed; quadrant test passed
  - `components/lcd_st7789/` — ST7789 SPI driver, 40 MHz, fill + fill_rect
  - Content displays 180° rotated with MADCTL=0x00 (needs mirror(true,true) before Stage 4 UI work)
  - Fixed black semicircle at physical top-right = hardware/panel artifact (not software)

## What Is Blocked

- [x] MADCTL orientation: `mirror(true,true)` added to `lcd_st7789.c` (Stage 4 session)
- [ ] SD ownership transition (Q3) — follows Q6
- [ ] Robot file-completion signal (Q6) — open
- [ ] Weld file format (Q8) — open
- [ ] Automated workbench flash workflow — Pi GPIO for Key1/Key2 wiring unverified

## What Is Next

1. **Stage 4 — flash and verify (build already done).**
   `BOARD=waveshare-esp32-s3-lcd-147 idf.py build` PASSED (1066/1066, 357 KB).
   Workbench Pi offline blocked flash — restore connectivity and flash via RFC2217.
   See session handoff above for exact procedure (steps 3–10).

2. **Add `workbench.local` to WSL `/etc/hosts`** — requires sudo;
   `echo "192.168.1.43 workbench.local" | sudo tee -a /etc/hosts`.

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
