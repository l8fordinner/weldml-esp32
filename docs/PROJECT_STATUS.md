# Project Status

Single source of truth for cross-session continuity.
Update this at the end of each working session and commit it with the session's changes.

---

## Current State (2026-06-19)

**Phase:** Stage 1 complete. RFC2217 monitor/reset workflow resolved. Ready for Stage 2.
No WeldML code written yet. Template firmware verified running on Waveshare ESP32-S3.

**Branch:** `main`
**Last committed:** `0f788e0`

**Board state:** SLOT3, `state=idle`, devnode=/dev/ttyACM1. Template firmware running (softAP "ESP32-Setup", HTTP on 192.168.4.1). IDF v5.3.2, no panic, no crash loop. OpenOCD active.

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
| Q1 | Port SmrtUsbEsp or rebase around it? | **Open** | | |
| Q2 | Native ESP-IDF or PlatformIO? | **Open** (follows Q1) | | |
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
- [x] Q9 resolved: `POST /api/flash` preferred path; RFC2217 fallback also confirmed
- [x] Q4 resolved: Pi HTTP portal confirmed; GPIO wiring confirmed (gpio_boot=18, gpio_en=17)
- [x] OpenOCD auto-started on SLOT3 (GDB 3335, telnet 4446, esp32s3-builtin.cfg)
- [x] `esp-idf-handling` skill covers full flash workflow — no new skill needed

## What Is Blocked

- [x] **Stage 1 complete** — template firmware flashed and boot-verified on Waveshare ESP32-S3
- [ ] `boards/waveshare-esp32-s3-lcd-147/` board config — blocked on Q1/Q2
- [ ] Any firmware components (LCD driver, SD init, inference engine) — blocked on Q1/Q2/Q6/Q8
- [ ] Automated workbench flash workflow — Pi GPIO wiring for Key1/Key2 still unverified

## What Is Next

1. **Stage 2** — Create `boards/waveshare-esp32-s3-lcd-147/` board config (16MB flash, PSRAM, correct GPIO defines). Say "proceed Stage 2" to start.
2. **Resolve Q1** — architecture decision (gut main.c + port TinyUSB/SD as components is the current plan; confirm to unlock Stage 3+).
3. **Add `workbench.local` to WSL `/etc/hosts`** — requires sudo; `echo "192.168.1.43 workbench.local" | sudo tee -a /etc/hosts`.

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
