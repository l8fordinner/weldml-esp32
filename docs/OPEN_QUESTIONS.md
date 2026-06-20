# Open Questions

Decisions that must be made before or during MVP implementation.
Update this file as questions are resolved.

---

## Q1: Port SmrtUsbEsp Into weldml-esp32 or Rebase Around It?

**Question:** Should WeldML firmware be built by porting SmrtUsbEsp code into this
`weldml-esp32` repo, or should `weldml-esp32` be rebased on top of the SmrtUsbEsp repo?

**Resolved (2026-06-19 — Stage 4):** Option A — port SmrtUsbEsp USB MSC + SD SPI
into `weldml-esp32` as new components.  `components/usb_msc_sd/` implements TinyUSB
CDC + MSC with SD SPI using `espressif/esp_tinyusb` managed component.  No fork or
rebase; `weldml-esp32` retains its template structure (OTA, MQTT, webserver) for
later milestones.  SmrtUsbEsp hardware pin assignments were the verified reference.

---

## Q2: Native ESP-IDF or PlatformIO?

**Question:** Should this project use native ESP-IDF (`idf.py`) or PlatformIO?

**Resolved (2026-06-19 — Stage 4, follows Q1):** Native ESP-IDF.  The project has
used `idf.py build / flash / monitor` since Stage 1 with no PlatformIO involvement.
IDF 5.3.2 + IDF Component Manager provides `espressif/esp_tinyusb` for TinyUSB, which
is the canonical path for USB MSC on ESP-IDF 5.x.  PlatformIO offers no advantage
for this project given the multi-board board.h system and Kconfig-gated components
already in place.

---

## Q3: Safe SD Ownership Transition from USB MSC to Firmware

**Question:** How does the firmware safely take ownership of the SD card after the host
robot finishes writing and unmounts (or the MSC session ends)?

**Context:**
- SmrtUsbEsp holds SD exclusively for the USB host while MSC is active.
- WeldML firmware must locally mount and read the SD card to process the weld file.
- Reading the SD while MSC is active would corrupt data or crash.

**Options:**
- A: Detect USB host eject (MSC media ejected callback) → trigger local mount + read.
- B: Listen for a CDC serial command from the robot → relinquish MSC → mount locally.
- C: Detect a quiet period (no writes for N seconds) as a heuristic completion signal.
- D: Robot presses a physical button (BOOT/Key1) after writing → triggers firmware read.
- E: Robot writes a sentinel file (e.g., `DONE.txt`) → firmware polls for it while idle.

**Resolved (2026-06-19 — Stage 5):** Option C — write-idle detection.
`tud_msc_write10_complete_cb` (TinyUSB weak symbol, overridden in `weld_processor.c`)
updates `s_last_write_ms` on every USB SCSI WRITE10 completion.  A FreeRTOS monitor
task polls every 250 ms; after 5000 ms of no writes, it calls
`tinyusb_msc_storage_mount(SD_MOUNT)` to take exclusive ESP ownership (sets
`is_fat_mounted=true`, which causes `tud_msc_test_unit_ready_cb` to return false and
`_msc_storage_write_sector` to reject host writes).  After processing, calls
`tinyusb_msc_storage_unmount()` to return block access to the USB host.
No USB re-enumeration.  No robot-side program changes required.
Known limitation: detects SCSI block-write idle, not a true Kawasaki file-close event.

---

## Q4: How Should the Pi Workbench Flash the Waveshare Board?

**Question:** What flash path should the Universal Embedded Workbench use for the
Waveshare ESP32-S3-LCD-1.47?

**Context:**
- The Waveshare board has native USB only (no UART chip). Port is `/dev/ttyACM0`.
- No auto-reset circuit — BOOT+RESET must be driven manually or via Pi GPIO.
- The prior board (HUZZAH32) used a different approach; that wiring should not be assumed.
- USB MSC mode (SmrtUsbEsp) occupies the native USB port during operation.

**Options:**
- A: Manual BOOT+RESET button presses (operator-assisted, no Pi GPIO needed).
- B: Pi GPIO lines wired to Key1 (GPIO0) and Key2 (EN) for automated boot/reset.
- C: CDC reboot command from Pi → firmware reboots into download mode.
- D: OpenOCD via ESP32-S3 native USB JTAG (no serial reset needed).

**Must verify before automating:**
- Which Pi GPIO lines (if any) are wired to this board.
- Voltage and polarity compatibility with 3.3 V pull-up on Key1/Key2.
- Review Universal Embedded Workbench repo wiring documentation.

**Resolved (2026-06-19):**
- Pi workbench confirmed reachable at 192.168.1.43 (`casey@PiEspWrkbench`).
- Board enumerates on Pi as `/dev/ttyACM0` in SLOT3 (`state: idle`).
- Full workbench HTTP portal confirmed: `http://192.168.1.43:8080/api/info` → `slots_configured=3`.
- Pi GPIO wiring confirmed: `gpio_boot=18` (→ Key1/BOOT/GPIO0), `gpio_en=17` (→ Key2/EN/RST).
- Automated download mode entry via `POST /api/gpio/set` is available; no manual BOOT+RESET required for flash.
- WSL→Pi flash path: `POST /api/flash` (Pi-side esptool). See Q9.
- OpenOCD auto-started on SLOT3: `debug_chip=esp32s3`, GDB port 3335, telnet 4446.

---

## Q5: Is Manual BOOT/RESET Sufficient for Early Work?

**Question:** Should the MVP development phase use manual button presses for flashing,
or invest in automated Pi GPIO boot/reset before writing MVP code?

**Recommendation:** Yes, manual is sufficient for early work. Automate only after the
MVP inference loop is working and the flash cycle frequency justifies it.

**Resolved (2026-06-19):** Manual BOOT+RESET confirmed working. Sequence verified:
hold Key1 (BOOT/GPIO0), press and release Key2 (RST/EN), release Key1. No auto-reset
circuit exists; this manual sequence is required for every flash cycle. Sufficient
for MVP development phase.

---

## Q6: What Signal Does the Robot Use to Indicate File Write Completion?

**Question:** After the robot writes a weld data file to the SD card via USB MSC,
what signal does it provide to indicate that the file is complete and ready to process?

**Options (not yet decided):**
- USB host ejects the MSC volume (OS-level unmount).
- Robot sends a CDC serial command over the USB CDC interface.
- Quiet period: no SD writes for a defined time interval.
- Physical button press (Key1/BOOT or an external signal).
- Robot writes a sentinel/marker file (e.g., `DONE`, `READY`, or a manifest).
- Convention: single file in a known location, overwritten each weld cycle.

**Impact:** The answer determines Q3 (SD ownership transition strategy) and shapes
the MSC callback and firmware state machine design.

**Resolved (2026-06-19 — Stage 5):** No robot-side signal required for Stage 5.
USB MSC write-idle detection (5000 ms quiet period) is the completion heuristic.
This decision was made explicitly: no Kawasaki pins, no GPIO handshaking, no DONE
file, no rename marker, no robot program changes.  See Q3.

---

## Q7: How to Integrate Screen Color Status into the Display Driver?

**Question:** The MVP requires white/red/green full-screen fills on the ST7789V3 LCD.
How should this be structured in firmware?

**Context:**
- Controller is likely ST7789V3 (possibly GC9307N on some units — verify on hardware).
- LCD SPI: MOSI GPIO45, SCLK GPIO40, CS GPIO42, DC GPIO41, RST GPIO39, BL GPIO48.
- Backlight is MOSFET-driven from GPIO48 (PWM-capable).
- ESP-IDF has `esp_lcd` component with ST7789 driver support.
- TinyUSB and SPI must coexist without bus conflicts.

**Options:**
- A: Use `esp_lcd` component (Espressif's official LCD driver framework).
- B: Write a minimal SPI-based ST7789V3 driver (small, no dependencies).
- C: Adapt an existing open-source ST7789 driver for ESP-IDF.

**Decision inputs:** Build system choice (Q2) and code base choice (Q1) affect which
driver options are compatible.

**Resolved (2026-06-19 — Stage 3):** Option A — `esp_lcd` component with the IDF-
bundled ST7789 panel driver (`esp_lcd_panel_st7789`).  Implemented in
`components/lcd_st7789/` as a thin wrapper.  Hardware-confirmed on Waveshare board.

---

## Q8: Weld File Format

**Question:** What is the exact format of the weld data file written to SD by the robot?

**Partially resolved (2026-06-20 — Stage 6 planning, from sample file inspection).**

File structure and column layout are confirmed from four training-dataset samples:

- `test_data/kawasaki_samples/GAP/NP/l314.fsj` (GAP, NP, PASS)
- `test_data/kawasaki_samples/GAP/IF/l320.fsj` (GAP, IF, FAIL)
- `test_data/kawasaki_samples/LOOCV/NP/l060.fsj` (LOOCV, NP, PASS)
- `test_data/kawasaki_samples/LOOCV/IF/l046.fsj` (LOOCV, IF, FAIL)

### File Format — Confirmed

**Extension:** `.fsj` (Kawasaki FSJ log format)  
**Encoding:** ASCII text, no BOM  
**Delimiter:** space-delimited with one leading space per data row  
**Sample rate:** 0.002 s time step = 500 Hz  

**File structure (in order):**
1. Header block: lines starting with `.*` — firmware versions, controller model (typically ~22 lines). Ignore for parsing.
2. `.LANGUAGE ENGLISH` keyword
3. `.FSJLOG` keyword
4. Timestamp line: ` [YY/MM/DD HH:MM:SS]` (2-digit year, space-padded)
5. Column header line: ` TIME LOADCELL GAP DEF  S.POS.M S.POS.LVDT T.AVE POS7  POS9 ICOM7 IFB7 IFB8  IFB9 VEL7 VEL8 STAGE`
6. Data rows: space-delimited, 16 fields per row, until footer begins
7. Footer: `***** F_FSJ PROCESSING RESULT *****` followed by stage parameters, then `.END`

**16 data columns (0-indexed after leading space):**

| Index | Name | Notes |
|-------|------|-------|
| 0 | TIME | Seconds; 0.002 s step; starts at 0.000 |
| 1 | LOADCELL | Force; units from sensor |
| 2 | GAP | Gap |
| 3 | DEF | Deflection |
| 4 | S.POS.M | Spindle position (motor) — **the SPOSM channel for segment windowing** |
| 5 | S.POS.LVDT | Spindle position (LVDT) |
| 6 | T.AVE | Tool average |
| 7 | POS7 | Position axis 7 |
| 8 | POS9 | Position axis 9 |
| 9 | ICOM7 | Current command axis 7 |
| 10 | IFB7 | Current feedback axis 7 |
| 11 | IFB8 | Current feedback axis 8 |
| 12 | IFB9 | Current feedback axis 9 |
| 13 | VEL7 | Velocity axis 7 |
| 14 | VEL8 | Velocity axis 8 (steady-state value ≈ rotation speed / 100) |
| 15 | STAGE | Weld stage integer: 1, 2, 3, 4, 5 |

**STAGE values:** STAGE starts at 1 (pre-plunge), transitions 1→2→3→4→5 during weld, then the footer begins with `*****`.  
**Stage 3** is the active stir dwell phase and defines the end of the feature extraction window.

**File sizes (from samples):**
- l314.fsj: 271 KB, 2610 lines, stage transition at ~time 3.42s
- l320.fsj: 526 KB, 5078 lines
- l060.fsj: 530 KB, 5029 lines
- l046.fsj: 257 KB, 2461 lines

**Footer ROTATE field:** Each stage line in the footer has `ROTATE = {RPM}` (e.g., `STAGE 1 ... ROTATE = 1800.00`). The programmed rotation speed is constant across all stage entries within a file (either 1400.00 or 1800.00 in the samples). Golden vectors confirm `RotationSpeed` = 1400.0 or 1800.0 exactly, consistent with the footer ROTATE parameter.

### Weld Window — Confirmed

Per `FEATURE_SCHEMA.json` and training command (`--segment-start sposm_ge_zero`):
- **Window start:** First data row where S.POS.M (column index 4) >= 0
- **Window end:** Last data row where STAGE (column index 15) == 3
- Feature extraction runs only on rows within this window.

For l314.fsj: window starts ~time 2.168 s, ends ~time 4.488 s (within Stage 3).

### Remaining Open Item → See Q10

File format is confirmed. The remaining blocker for Stage 6B (feature extraction) is:
**Which FSJ column(s) are the primary signal source for time-domain, FFT, and CWT features?**  
This is documented in Q10. Q8 is closed for file structure/format.

---

## Q9: WSL → Pi Flash Path

## Q9: WSL → Pi Flash Path

**Question:** How does `idf.py flash` reach `/dev/ttyACM0` on the Pi from WSL?
The Pi has esptool but no ESP-IDF. WSL has ESP-IDF but the board is on the Pi USB hub.

**Context:**
- Board is connected to Pi USB hub, not directly to the Windows/WSL machine.
- ESP-IDF v5.3.2 is in WSL; esptool v5.2.0 is on the Pi.
- The Pi workbench may expose an RFC2217 serial proxy (unverified; `ps aux | grep rfc2217` not yet run).
- Alternatively, the binary can be copied to the Pi and flashed via SSH + esptool.

**Options:**
- A: RFC2217 proxy on Pi → `idf.py -p rfc2217://192.168.1.43:<port> flash` from WSL.
- B: `idf.py build` in WSL, then `scp build/*.bin casey@192.168.1.43:~/ && ssh ... esptool write-flash`.
- C: Install ESP-IDF on Pi and run `idf.py flash` entirely on Pi (over SSH).
- D: OpenOCD via ESP32-S3 native USB JTAG from WSL (USB passthrough not yet set up).

**Flash size note:** Build uses `--flash_size 4MB` (root `sdkconfig.defaults`). This is
**not a blocker for a first smoke-test flash** — `partitions.csv` fits within 4MB and the
chip's upper 12MB is simply unused at runtime. Correct fix is to add
`CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y` to the future `boards/waveshare-esp32-s3-lcd-147/sdkconfig.defaults`.
Do not change root `sdkconfig.defaults` or `boards/esp32-s3/sdkconfig.defaults`.
Required before: OTA expansion, SPIFFS above 4MB, or production WeldML firmware deployment.

**Resolved (2026-06-19):**
- Full workbench HTTP portal running at `http://192.168.1.43:8080`.
- Waveshare board on SLOT3: `url=rfc2217://192.168.1.43:4003`, `devnode=/dev/ttyACM0`, `state=idle`.
- Preferred flash path: `POST /api/flash` with multipart `flash_args` + .bin files. Portal runs esptool
  on the Pi directly, handles GPIO download mode automatically (gpio_boot=18, gpio_en=17).
- Fallback: `idf.py -p rfc2217://192.168.1.43:4003 flash` from WSL (confirmed working via chip_id test).
- Monitor: `idf.py -p rfc2217://192.168.1.43:4003 monitor` from WSL.
- Hostname alias needed in WSL `/etc/hosts`: `192.168.1.43 workbench.local`.

---

## Q10: FSJ Feature Channel Mapping

**Question:** Which FSJ data column (or footer field) is the primary signal source for each of the 22 model features?

**Context:**
- Q8 closed for file format/structure. The 16 FSJ column names are confirmed.
- `FEATURE_SCHEMA.json` describes features by purpose ("time-domain mean", "FFT bandwidth", etc.) but does not name the source FSJ column.
- `RotationSpeed` in golden vectors is exactly 1400.0 or 1800.0 — consistent with `ROTATE` in the `***** F_FSJ PROCESSING RESULT *****` footer (e.g., `STAGE 1 ... ROTATE = 1800.00`). Whether the training code reads the footer directly or derives it from VEL8 × 100 is unconfirmed.
- `MinPositionStage3` = 2.29 for l314.fsj in golden vectors; S.POS.M reaches ~+2.29 during Stage 3, making S.POS.M the likely source for position features.
- `Mean`, `RMS`, `PeakValue`, `StandardDeviation`, `ClearanceFactor`, `CrestFactor`, `ImpulseFactor`, `ShapeFactor` — described as "time-domain force/position signal statistic" in FEATURE_SCHEMA. Likely LOADCELL but not confirmed.
- `FFT_*` and `CWT_*` features — derived from a primary signal (likely LOADCELL) but signal source not confirmed.
- `PlungeVelocity` — could be VEL7, computed from dS.POS.M/dt, or from the footer P.SPD parameter.
- `MaxForceBelow3mm` — "maximum force below 3 mm plunge/position threshold"; position column for the 3 mm threshold is unconfirmed.

**Needed to implement Stage 6B (feature extraction).**  
**Authoritative source:** `src/weldmltrainer/feature_extraction.py` from the WeldML trainer repository.

**Blocker:** Stage 6B must not be coded until Q10 is answered by reading feature_extraction.py.  
Stage 6A (file discovery and parser contract for structure/windowing) is NOT blocked by Q10.

---

## Resolution Log

| Q | Status | Decision | Date |
|---|--------|----------|------|
| Q1 | **Resolved** | Port SmrtUsbEsp code as new components into weldml-esp32 (Option A); no fork | 2026-06-19 |
| Q2 | **Resolved** | Native ESP-IDF (`idf.py`); follows Q1; in use since Stage 1 | 2026-06-19 |
| Q3 | **Resolved** | Write-idle detection (5000 ms) + tinyusb_msc_storage_mount/unmount for exclusive ESP access | 2026-06-19 |
| Q4 | **Resolved** | Pi workbench HTTP portal confirmed; GPIO wiring confirmed (gpio_boot=18, gpio_en=17); automated download mode works | 2026-06-19 |
| Q5 | **Resolved** | Manual BOOT+RESET works; automated GPIO path also confirmed; sufficient for MVP dev phase | 2026-06-19 |
| Q6 | **Resolved** | No robot signal required; 5000 ms write-idle is the completion indicator (Stage 5 decision) | 2026-06-19 |
| Q7 | **Resolved** | `esp_lcd` component + ST7789 panel driver; implemented in `components/lcd_st7789/` | 2026-06-19 |
| Q8 | **File format resolved; Q10 split out** | .fsj ASCII text, 16 columns, S.POS.M>=0 to last STAGE==3 defines weld window | 2026-06-20 |
| Q9 | **Resolved** | Preferred: `POST /api/flash` (Pi-side esptool); fallback: `idf.py -p rfc2217://192.168.1.43:4003 flash` | 2026-06-19 |
| Q10 | **Open — blocks Stage 6B** | FSJ column → feature mapping; source is feature_extraction.py | 2026-06-20 |
