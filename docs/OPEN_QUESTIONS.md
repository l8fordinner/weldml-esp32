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

**Status: FULLY RESOLVED (2026-06-20) — Stage 6B is now UNBLOCKED.**

**Sources:** `model_exports/esp32_port/feature_extraction.py` + `model_exports/esp32_port/parse_and_clean.py`

---

### Complete Resolution (2026-06-20 — parse_and_clean.py inspection)

All four prior blockers are now answered.

---

### 1. Signal columns and preprocessing — CONFIRMED

**Primary signal for time-domain, FFT, and CWT features:**
- `load_values` = LOADCELL column (index 1) — raw, no preprocessing whatsoever.
- `time_values` = TIME column (index 0).
- `parse_and_clean._compute_features` passes `load_series.loc[start_idx:end_idx].dropna().to_numpy()` directly to `build_feature_vector`.
- **No smoothing, no filtering, no baseline correction, no unit conversion.** Raw LOADCELL values.

**Position column used for position-based features:**
- `_select_position_column` iterates `("POS7", "POSITION", "S.POS.M", "S.POS.LVDT", "POS9")` and returns the first match.
- FSJ files have POS7 (index 7) — it is chosen first. POSITION = POS7 throughout.

**RotationSpeed (feature index 0):**
- Footer `STAGE N ... ROTATE = {RPM}` — first match wins (via `ROTATE_RE`).
- **Confirmed: RotationSpeed = float(footer ROTATE) = 1400.0, 1600.0, or 1800.0.**

---

### 2. Segment window rule — CONFIRMED

```
start_idx = first row index where S.POS.M >= 0  (sposm_ge_zero mode)
end_idx   = last row index where STAGE == 3

load_segment = load_series.loc[start_idx : end_idx].dropna()
time_segment = time_series.loc[start_idx : end_idx].dropna()
```

- **Inclusive on both ends** (pandas `.loc` label-based slice with RangeIndex).
- `.dropna()` is called; has no effect when all LOADCELL/TIME values are numeric (confirmed for all four fixtures).
- For l314.fsj: start_idx = 1084, end_idx = 2244, 1161 rows — matches Stage 6A parser exactly.

---

### 3. MaxForceBelow3mm (feature index 14) — CONFIRMED

```python
mask = (POS7 < 3.0) AND (STAGE IN [2, 3])           # applied to FULL dataset
MaxForceBelow3mm = max(LOADCELL[mask])
```

- Uses the FULL file (not just the window), all rows where STAGE is 2 or 3 AND POS7 < 3.0.
- Position column is POS7 (confirmed by `_select_position_column` priority order).
- **Verified against l314.fsj:** computed = 14.689, golden = 14.689. **Exact match.** ✓

---

### 4. PlungeVelocity (feature index 18) — CONFIRMED

```python
# STAGE==2 rows only:
t_30 = interpolated TIME when POS7 crosses 3.0 mm (linear interp through sign change)
t_25 = interpolated TIME when POS7 crosses 2.5 mm (linear interp through sign change)
PlungeVelocity = 0.5 / |t_25 - t_30|
```

- Uses linear interpolation (`_interpolated_time_at_position`) through consecutive STAGE==2 rows where position crosses the threshold.
- Falls back to nearest sample if no sign-change crossing exists.
- Units: mm / s (0.5 mm span divided by time interval in seconds).
- **Verified against l314.fsj:** t_30 = 2.884, t_25 = 3.196, computed = 1.602564, golden = 1.602564. **Exact match.** ✓

---

### 5. MinPositionStage3 (feature index 16) — RE-CONFIRMED WITH EXACT TRAINER CODE

```python
stage3_df = rows where (STAGE == 3) AND (POS7 < 3.0)
MinPositionStage3 = min(stage3_df["POSITION"])   # POSITION = POS7
```

- Only STAGE==3 rows where POS7 < 3.0 are considered. In practice, for all fixtures the minimum POS7 during STAGE==3 is well below 3.0, so the filter has no effect.
- **Verified against l314.fsj:** computed = 2.29, golden = 2.29. **Exact match.** ✓

---

### 6. Parity gap — ROOT CAUSE IDENTIFIED AND CLOSED

The prior-session observation (mean=12.679 vs golden 12.947, peak=14.730 vs golden 16.44) was real but is NOT a trainer code issue.

**Root cause: the test fixture l314.fsj in `test_data/kawasaki_samples/` is a DIFFERENT FILE from the l314.fsj used to generate the golden vectors.**

Evidence:
- Position-based features (MinPositionStage3, MaxForceBelow3mm, PlungeVelocity) all match golden EXACTLY — these depend on POS7, STAGE, and TIME, confirming it is the same weld event and conditions.
- LOADCELL-based features (Mean, RMS, PeakValue) do NOT match — the LOADCELL values in the window differ between our fixture and the original training file.
- Running the trainer's `_compute_features` on our fixture with the exact trainer code produces the same mismatched values (mean=12.679, peak=14.730), confirming the trainer code is correct and our fixture's LOADCELL column simply has different values.
- The golden vectors were sourced from `data_processed\WSU_Tm_Ans_Dat_gap.csv` (training machine path); the original FSJ files for that run are not present in this repo.

**Conclusion:** The formulas are correct and confirmed. The fixture LOADCELL signal reflects a different data export or sensor calibration. No preprocessing transformation is missing.

**Implication for testing:**
- Feature extraction formulas CAN be implemented from the confirmed code.
- End-to-end numerical validation of LOADCELL-based features against golden is NOT possible with the current fixture files.
- For inference verification (Stage 6C), use golden feature vectors directly from `golden_vectors.csv` as model inputs — this bypasses the LOADCELL discrepancy and validates inference logic only.

---

### Complete Feature Formula Summary

| Feature | Source | Formula | Verified |
|---------|--------|---------|---------|
| RotationSpeed | Footer | `float(ROTATE=...)` first footer match | ✓ |
| Mean | LOADCELL window | `np.mean(load_values)` | formula ✓ |
| RMS | LOADCELL window | `sqrt(mean(load_values²))` | formula ✓ |
| StandardDeviation | LOADCELL window | `np.std(load_values, ddof=1)` | formula ✓ |
| PeakValue | LOADCELL window | `max(abs(load_values))` | formula ✓ |
| ShapeFactor | LOADCELL window | `RMS / mean(abs(load_values))` | formula ✓ |
| CrestFactor | LOADCELL window | `PeakValue / RMS` | formula ✓ |
| ClearanceFactor | LOADCELL window | `PeakValue / mean(sqrt(abs(load_values)))²` | formula ✓ |
| ImpulseFactor | LOADCELL window | `PeakValue / mean(abs(load_values))` | formula ✓ |
| FFT_* (5 features) | LOADCELL window, TIME | demeaned, zero-padded to 4096, rfft | formula ✓ |
| CWT_* (5 features) | LOADCELL window | Morlet, scales=[1,2,4,8,16,32] | formula ✓ |
| MinPositionStage3 | POS7, STAGE | `min(POS7 where STAGE==3 AND POS7 < 3.0)` | exact ✓ |
| MaxForceBelow3mm | LOADCELL, POS7, STAGE | `max(LOADCELL where POS7 < 3.0 AND STAGE IN [2,3])` | exact ✓ |
| PlungeVelocity | POS7, TIME, STAGE | `0.5 / \|t(POS7=2.5mm) − t(POS7=3.0mm)\|` in STAGE==2 | exact ✓ |

**Stage 6B is unblocked. Implement feature extraction per the formulas above.**

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
| Q10 | **Resolved — Stage 6B UNBLOCKED** | All 22 feature formulas confirmed from parse_and_clean.py: signal=raw LOADCELL, window=sposm_ge_zero to last STAGE3 inclusive, MaxForceBelow3mm=max(LOADCELL where POS7<3 AND STAGE∈[2,3]), PlungeVelocity=0.5/\|t(2.5mm)−t(3.0mm)\| in STAGE2. Parity gap explained: fixture LOADCELL values differ from training file (position-based features match exactly). | 2026-06-20 |
