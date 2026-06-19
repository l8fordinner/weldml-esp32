# Open Questions

Decisions that must be made before or during MVP implementation.
Update this file as questions are resolved.

---

## Q1: Port SmrtUsbEsp Into weldml-esp32 or Rebase Around It?

**Question:** Should WeldML firmware be built by porting SmrtUsbEsp code into this
`weldml-esp32` repo, or should `weldml-esp32` be rebased on top of the SmrtUsbEsp repo?

**Context:**
- SmrtUsbEsp already has working TinyUSB MSC + CDC, SD SPI init, and WS2812B LED on this
  exact board with confirmed pin assignments.
- This `weldml-esp32` repo has a more complete base template (OTA, MQTT, webserver,
  provisioning, SPIFFS web UI, multi-board structure, partition table).
- The MVP does not need MQTT, OTA, or web UI — but those are desired for later milestones.

**Options:**
- A: Port SmrtUsbEsp USB MSC + SD + LED code into `weldml-esp32` as new components.
- B: Fork SmrtUsbEsp and layer the WeldML inference + LCD code on top of it.
- C: Start fresh, referencing both repos for the relevant pieces.

**Pending until:** Architecture decision session before coding begins.

---

## Q2: Native ESP-IDF or PlatformIO?

**Question:** Should this project use native ESP-IDF (`idf.py`) or PlatformIO?

**Context:**
- This repo (`weldml-esp32`) is built on native ESP-IDF (CMake, `idf_component.yml`,
  Kconfig, partition CSV). See REQUIREMENTS.md for rationale.
- SmrtUsbEsp uses PlatformIO + ESP-IDF (platformio.ini wraps idf.py).
- PlatformIO may lag behind on ESP-IDF version support and has less Kconfig flexibility.
- Native ESP-IDF is the more capable path for multi-target, Kconfig-gated components.
- PlatformIO is simpler for initial setup and has good VS Code integration.

**Pending until:** Q1 resolved (the answer may follow from the code base choice).

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

**Pending until:** File completion signal decided (see Q6).

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

**Pending until:** Confirmed with robot/host software owner.

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

**Pending until:** Q1 and Q2 resolved.

---

## Q8: Weld File Format

**Question:** What is the exact format of the weld data file written to SD by the robot?

**Needed to implement:**
- File parser in firmware.
- Feature extraction from the parsed data.
- File naming convention and directory structure on SD.

**Pending until:** File format specification provided by WeldML project owner.

---

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

## Resolution Log

| Q | Status | Decision | Date |
|---|--------|----------|------|
| Q1 | Open | | |
| Q2 | Open | | |
| Q3 | Open | | |
| Q4 | **Resolved** | Pi workbench HTTP portal confirmed; GPIO wiring confirmed (gpio_boot=18, gpio_en=17); automated download mode works | 2026-06-19 |
| Q5 | **Resolved** | Manual BOOT+RESET works; automated GPIO path also confirmed; sufficient for MVP dev phase | 2026-06-19 |
| Q6 | Open | | |
| Q7 | Open | | |
| Q8 | Open | | |
| Q9 | **Resolved** | Preferred: `POST /api/flash` (Pi-side esptool); fallback: `idf.py -p rfc2217://192.168.1.43:4003 flash` | 2026-06-19 |
