# Project Status

Single source of truth for cross-session continuity.
Update this at the end of each working session and commit it with the session's changes.

---

## Current State (2026-06-19)

**Phase:** Pre-implementation — context setup and documentation complete.
No firmware code has been written yet. All firmware work is blocked on Q1 and Q2.

**Branch:** `main` (behind origin — 4 docs files created this session, not yet committed)
**Last committed:** `99003ef` "Add hardware docs and project context"

### Pending commit (run to stabilize)

```bash
git add docs/CODING_GUIDELINES.md docs/CONTEXT_POLICY.md docs/MODEL_ROUTING.md docs/PROJECT_STATUS.md
git commit -m "Add operating setup: guidelines, context policy, model routing, project status"
git push
```

### Session note (2026-06-19)

Context auto-compacted mid-session before the above commit could be staged (auto-mode
classifier blocked `git add` while "wait for approval" text was still in the plan context).
All 4 files are complete and correct. `CONTEXT_POLICY.md` updated this session to add
explicit percentage thresholds (70%/80%/85%) and updated handoff format.

---

## Open Questions — Resolution Log

See `docs/OPEN_QUESTIONS.md` for full context on each question.

| Q | Title | Status | Decision | Date |
|---|-------|--------|----------|------|
| Q1 | Port SmrtUsbEsp or rebase around it? | **Open** | | |
| Q2 | Native ESP-IDF or PlatformIO? | **Open** (follows Q1) | | |
| Q3 | SD ownership transition (MSC → firmware) | **Open** (follows Q6) | | |
| Q4 | Pi workbench flash path for Waveshare | **Open** | | |
| Q5 | Manual BOOT/RESET sufficient for early work? | **Open** (likely yes) | | |
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
| **Waveshare ESP32-S3-LCD-1.47** | — | — | — | **Not yet tested** | Awaiting Q1/Q2 decision |

---

## Workbench Status

| Item | Status |
|------|--------|
| Workbench Pi available | Unknown — not yet confirmed with Waveshare board |
| Pi sees Waveshare on /dev/ttyACM0 | **Unverified** |
| Pi boot/reset GPIO wiring for Waveshare | **Unverified** — do not assume HUZZAH32 wiring applies |
| Manual BOOT+RESET sequence tested | **Not yet** |
| UDP log capture on Waveshare | **Not yet** |

---

## What Is Done

- [x] Hardware documentation extracted from schematic and datasheet (see docs/)
- [x] Verified pin table for Waveshare ESP32-S3-LCD-1.47 (schematic + SmrtUsbEsp confirmation)
- [x] Project context docs created: PRIOR_WORK_CONTEXT, MVP_REQUIREMENTS, DEBUG_WORKBENCH_CONTEXT, OPEN_QUESTIONS
- [x] Operating rules: CLAUDE.md (local), CONTEXT_POLICY.md, CODING_GUIDELINES.md, MODEL_ROUTING.md
- [x] PROJECT_STATUS.md created (this file)

## What Is Blocked

- [ ] `boards/waveshare-esp32-s3-lcd-147/` board config — blocked on Q1/Q2
- [ ] Any firmware components (LCD driver, SD init, inference engine) — blocked on Q1/Q2/Q6/Q8
- [ ] Automated workbench flash workflow — blocked on Q4/Q5 (workbench wiring unverified)

## What Is Next

1. **Resolve Q1 and Q2** — decide whether to port SmrtUsbEsp into this repo or rebase on it,
   and whether to use native ESP-IDF or PlatformIO. Update resolution log above when decided.
2. **Create board config** for Waveshare ESP32-S3-LCD-1.47 once Q1/Q2 are resolved.
3. **First hardware test on Waveshare board** — manual BOOT+RESET flash, boot log via monitor.
4. **Validate workbench** with Waveshare board (Q4/Q5).

---

## Known Conflicts to Resolve Before First Build

| Issue | File | Current value | Correct value |
|-------|------|--------------|--------------|
| Flash size | `sdkconfig.defaults` | 4 MB | 16 MB (W25Q128JVSI) |
| PSRAM not enabled | `sdkconfig.defaults` | not set | CONFIG_SPIRAM=y, CONFIG_SPIRAM_MODE_OCT=y |
| Generic SPI pins | `boards/esp32-s3/board.h` | MOSI=11, CLK=12, CS=10 | LCD: MOSI=45, CLK=40, CS=42; SD: CLK=14, MOSI=15, CS=21 |
| LED pin conflict | `boards/esp32-s3/board.h` | BOARD_LED_PIN=48 | GPIO48 is LCD backlight — cannot be used as LED |
| Partition table | `partitions.csv` | 4 MB layout | Needs 16 MB layout for Waveshare board |
