# NOTES.md — Testing Status

Tracks what has been verified on hardware and what still needs end-to-end testing.
Update this file as tests are completed.

Board used: **Adafruit Feather HUZZAH32** (ESP32-WROOM-32, chip rev v3.0)
Connected: `/dev/ttyUSB0` via CP2104 USB-serial (usbipd passthrough from Windows)

---

## Verified

| Test | Result | Notes |
|------|--------|-------|
| `idf.py build` | PASS | Clean build, 986 objects, ~963 KB binary, 42% OTA partition headroom |
| `idf.py flash` | PASS | 987 KB flashed + verified at 541 kbit/s |
| Board reachable on /dev/ttyUSB0 | PASS | esptool identifies ESP32-D0WD-V3 rev v3.0 |
| Boot log via serial | PASS | Full boot log captured; IDF v5.3.2, partition table correct |
| OTA partition table | PASS | nvs / otadata / ota_0 / ota_1 / spiffs at correct offsets |
| SoftAP first-run mode | PASS | `ESP32-Setup` visible in WiFi list, DHCP assigns 192.168.4.1 |
| Web UI served from SPIFFS | PASS | HTML/CSS/JS pages load at http://192.168.4.1 after SPIFFS flash |
| Factory reset via web API | PASS | POST /api/factory-reset erases NVS, reboots into SoftAP mode. Tested via workbench Pi. |
| WiFi provisioning via web UI | PASS | POST /api/wifi saves SSID/password to NVS, DUT reboots into STA mode. |
| NVS persistence across reboots | PASS | Credentials survive hardware RST; DUT reconnects to provisioned AP. |
| Web UI accessible in STA mode | PASS | All 4 HTML pages (/, /config, /ota, /status) return HTTP 200 in STA mode. |
| MQTT connection to broker | PASS | mqtt_url saved to NVS, DUT connects on boot; verified via mosquitto $SYS topic (1 client). |
| OTA update trigger via web UI | PASS | POST /api/ota triggers download from http://192.168.4.1:8080; DUT rebooted to ota_1 partition. Requires CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP=y. |

**Note on serial monitor:** `idf.py monitor` requires an interactive TTY and cannot
be used from Claude Code directly. Use a separate terminal or VS Code serial monitor
for interactive monitoring. Claude Code uses pyserial for one-shot log capture.

---

## Waveshare ESP32-S3-LCD-1.47 — Hardware Validation (2026-06-19)

Board connected to Pi workbench (`PiEspWrkbench`, 192.168.1.43) via USB hub.
Board MAC: `98:3d:ae:e4:4e:ac`

| Test | Result | Notes |
|------|--------|-------|
| Pi workbench reachable | PASS | `ssh casey@192.168.1.43` via key auth from WSL |
| Board enumeration on Pi — CDC | PASS | `/dev/ttyACM0` visible in SmrtUsbEsp run mode |
| Board enumeration on Pi — MSC | PASS | `sda` / `sda1` (29 GiB SD card) via `lsblk`; TinyUSB Flash Storage 0.2 |
| BOOT+RESET enters download mode | PASS | Manual button sequence confirmed; automated via GPIO also available (see below) |
| Pi GPIO wiring — BOOT (gpio_boot=18) | CONFIRMED | Pi GPIO18 → Key1 (BOOT/GPIO0); verified via `/api/devices` SLOT3 |
| Pi GPIO wiring — EN (gpio_en=17) | CONFIRMED | Pi GPIO17 → Key2 (EN/RST); verified via `/api/devices` SLOT3 |
| Download mode USB enumeration | PASS | idVendor=303a, idProduct=1001 (USB JTAG/serial debug unit, Espressif) |
| Chip: ESP32-S3 QFN56 rev0.2 | PASS | Verified via `esptool chip-id` (from Pi) |
| PSRAM: 8MB embedded (AP_3v3) | PASS | Verified via esptool features line |
| Flash: 16MB Winbond W25Q128 | PASS | Verified via `esptool flash-id`: manufacturer=ef, device=4018, 3.3V quad SPI |
| Crystal: 40MHz | PASS | Verified via esptool |
| WSL ESP-IDF v5.3.2 | PASS | `idf.py --version` after sourcing `~/esp/esp-idf/export.sh` |
| Pi ESP-IDF | NOT INSTALLED | Pi-side esptool used via `POST /api/flash`; WSL IDF only needed for build |
| Pi workbench HTTP portal | CONFIRMED | `http://192.168.1.43:8080/api/info` returns host_ip, slots_configured=3 |
| Waveshare assigned to SLOT3 | CONFIRMED | `/api/devices`: state=idle, devnode=/dev/ttyACM0, url=rfc2217://192.168.1.43:4003 |
| OpenOCD auto-started (SLOT3) | CONFIRMED | debugging=true, debug_chip=esp32s3, gdb_port=3335, telnet=4446 |
| Build for esp32s3 target | PASS | `idf.py build` — 1028/1028 targets, zero errors, zero warnings |
| Flash (template firmware) | PASS — binary verified | `idf.py -p rfc2217://192.168.1.43:4003 flash` from WSL; SHA hash verified all 5 binaries; esptool v4.11.0; 2026-06-19 |
| Boot log capture via portal API | PASS | `POST /api/serial/reset` (with OpenOCD stopped) returns full boot log via DTR/RTS path; 76 lines captured 2026-06-19 |

**Flash path (confirmed 2026-06-19):**
- `POST /api/flash` endpoint does NOT exist on this portal version. Actual path:
  `idf.py -p rfc2217://192.168.1.43:4003 flash` from WSL (IDF venv python required).
- esptool's `--before default_reset` works via RFC2217 DTR/RTS passthrough for ESP32-S3 USB Serial/JTAG.
- IDF venv invocation: `IDF_PATH=/home/casey/esp/esp-idf IDF_PYTHON_ENV_PATH=~/.espressif/python_env/idf5.3_py3.12_env ~/.espressif/python_env/idf5.3_py3.12_env/bin/python ~/esp/esp-idf/tools/idf.py -p rfc2217://192.168.1.43:4003 flash`
- Add `/etc/hosts` entry `192.168.1.43 workbench.local` in WSL (requires sudo; not yet added).

**Boot log (2026-06-19, Stage 1 PASS):**
```
rst:0x15 (USB_UART_CHIP_RESET),boot:0x8 (SPI_FAST_FLASH_BOOT)
ESP-IDF v5.3.2 2nd stage bootloader
chip revision: v0.2 | SPI Flash Size: 4MB (W: size 16MB > header 4MB — expected, Stage 2 fix)
App: esp32-base-template v08fcb1e compiled Jun 19 2026 04:30:34
"ESP32 Base Template starting — IDF vv5.3.2"
WiFi softAP "ESP32-Setup" at 192.168.4.1
SPIFFS mounted: 12550/534881 bytes used
HTTP server started on port 80
"Startup complete"
```
No panic. No crash loop. Template firmware running correctly on Waveshare ESP32-S3.

**RFC2217 monitoring note (resolved 2026-06-19):**

After RST, board re-enumerated as `/dev/ttyACM1` (not `/dev/ttyACM0`).
Portal auto-restarted RFC2217 server on `/dev/ttyACM1:4003`.

Root cause analysis: `plain_rfc2217_server.py` correctly sets DTR=False/RTS=False on startup
and after each client disconnect. The download-mode trigger was two separate issues:

1. **JTAG path skips boot log:** When OpenOCD is running (debugging=true), `POST /api/serial/reset`
   uses `reset run` via telnet — which resets the CPU but returns JTAG output, not serial boot log.
   The DTR/RTS path (which captures boot output) is only taken when OpenOCD is stopped.

2. **idf.py monitor asserts DTR=True via RFC2217:** The IDF monitor's pyserial client sends
   `SET_CONTROL DTR_ON` during RFC2217 negotiation. The PortManager passes this through to
   `/dev/ttyACM1`, causing the ESP32-S3 USB-JTAG peripheral to hold GPIO0 LOW. If the board
   resets while the monitor is connected, it enters download mode.

**Verified repeatable workflow (2026-06-19):**

```bash
# Flash (unchanged)
IDF_PATH=... idf.py -p rfc2217://192.168.1.43:4003 flash

# Capture boot log via portal (stop OpenOCD first to use DTR/RTS path)
curl -X POST http://192.168.1.43:8080/api/debug/stop \
  -H 'Content-Type: application/json' -d '{"slot": "SLOT3"}'
curl -X POST http://192.168.1.43:8080/api/serial/reset \
  -H 'Content-Type: application/json' -d '{"slot": "SLOT3"}'
# → Full boot log returned in JSON "output" array
curl -X POST http://192.168.1.43:8080/api/debug/start \
  -H 'Content-Type: application/json' -d '{"slot": "SLOT3"}'

# Interactive monitor: --no-reset prevents DTR=True assertion
idf.py -p rfc2217://192.168.1.43:4003 monitor --no-reset
```

Boot log verified via DTR/RTS reset on 2026-06-19: IDF v5.3.2, no panic, softAP up, "Startup complete".
Full 76-line boot log captured in single API call. This is a workbench infrastructure behavior, not a firmware bug.

**Flash size note:** Build uses `--flash_size 4MB` (from root `sdkconfig.defaults`).
Physical flash is 16MB (Winbond W25Q128). This is **not a blocker for a first smoke-test
flash**: `partitions.csv` ends at 0x3f0000 (3.94 MB) and fits entirely within 4MB; the
upper 12MB is simply unused at runtime. No data goes to wrong addresses; the firmware
will boot and run correctly. The correct fix is to create
`boards/waveshare-esp32-s3-lcd-147/sdkconfig.defaults` with
`CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y` as part of board-specific setup (blocked on Q1/Q2).
Do not change root `sdkconfig.defaults` or `boards/esp32-s3/sdkconfig.defaults`.
Required before: OTA layout expansion, SPIFFS growth above 4MB, or real WeldML firmware deployment.

---

## Performance Investigation — Feature Extraction (2026-07-23)

### Context

Stage 6C inference is complete. Hardware test with `l060.fsj` (LOOCV/NP, 1854-sample window)
showed correct classification (NP, probability_class1=0.0) but ~2 minute total processing time.
Target is <10 seconds end-to-end.

### Baseline measurement (timing build, 2026-07-23)

Timing fields added to `weldml_result.json` and `weldml_results.csv`:
- `parse_ms` — time for `fsj_parse_file()` (SD I/O + parsing)
- `features_ms` — time for `fsj_extract_features()` (time-domain + FFT + CWT)

**Result (l060.fsj, window_count=1854, 500 Hz, 3.708 s of weld data):**

| Field | Value |
|-------|-------|
| `parse_ms` | 1,905 ms |
| `features_ms` | 122,113 ms |
| Total | ~124 seconds |
| Predicted class | 0 (NP) — correct for LOOCV/NP fixture |
| `MinPositionStage3` | 1.86 (triggers first tree node ≤2.185 → NP directly) |
| `FFT_FrequencyBandwidth` | 33.18 (not reached in inference; logged for traceability) |

**Bottleneck: `features_ms` dominates at 64× parse time.**

Root cause in `compute_fft_features()` (`components/weld_parser/weld_parser.c`):
- Naive O(N²) DFT: outer loop over `power_count = FFT_PAD_LENGTH/2 + 1 = 2049` bins,
  inner loop over `used = min(window_count, 4096) = 1854` samples.
- Total: 2049 × 1854 = **~3.8 million** `cosf`/`sinf` calls per weld file.
- Hardware FPU on Xtensa LX7 assists arithmetic but `cosf`/`sinf` are still software
  polynomial approximations — not accelerated by SIMD.

CWT (`compute_cwt_features()`) is fast relative to FFT — not investigated yet but
expected to be O(n × Σkernel_len) ≈ 1.88M MACs with float, dominated by multiply-accumulate
rather than trig. The 122 s `features_ms` is almost entirely FFT.

### Optimization 1 — esp-dsp radix-2 FFT (planned)

**Change:** Replace the naive O(N²) DFT with `dsps_fft2r_fc32` from the `espressif/esp-dsp`
managed component.

- Algorithm: Cooley-Tukey radix-2 FFT, O(N log N).
- For N=4096: ~4096 × 12 = **~49K** complex butterfly operations (vs. 3.8M trig calls).
- Theoretical ops reduction: ~77×. Additional benefit: Xtensa SIMD (dual-MAC).
- Expected `features_ms` after change: <2 seconds (target), likely 500–2000 ms.

**Numerical equivalence:** The FFT of the zero-padded demeaned LOADCELL signal
is mathematically identical to the naive DFT. Feature values should match to float32
precision. Classification result must be the same (NP for l060.fsj).

**Implementation approach:**
- `idf_component.yml`: add `espressif/esp-dsp: "*"`
- `components/weld_parser/CMakeLists.txt`: add `esp-dsp` to `PRIV_REQUIRES`
- `components/weld_parser/weld_parser.c`:
  - `#ifdef ESP_PLATFORM`: include `esp_dsp.h`, call `dsps_fft2r_init_fc32(NULL, 4096)` once,
    then `dsps_fft2r_fc32` + `dsps_bit_rev_fc32` on a 4096×2 interleaved float buffer.
  - `#else` (host/test path): retain existing naive DFT unchanged.
- Power spectrum extraction is identical post-FFT; all 5 feature formulas unchanged.

### Results after Optimization 1 (verified on hardware 2026-07-23)

| Field | Expected | Actual |
|-------|----------|--------|
| `parse_ms` | ~1,900 ms (unchanged) | 2,004 ms ✓ |
| `features_ms` | <2,000 ms | **3,859 ms** |
| Speedup | ~60–100× | **31.6×** (122,113 → 3,859 ms) |
| Classification result | NP (unchanged) | NP ✓ — `FFT_FrequencyBandwidth=33.18` identical |

**Total processing time: ~5.9 seconds** (was ~124 seconds). Under 10-second target. ✓

The speedup was lower than the theoretical ~77× ops reduction because:
- The naive DFT's inner loop over `used=1854` (not 4096) reduced its actual work vs. worst-case.
- `compute_cwt_features()` is now a significant share of `features_ms` (~3.9 s includes both FFT and CWT).
- If further speedup is needed, CWT is the next candidate (O(n × Σkernel_len) ≈ 1.88M MACs per weld).

**NP result blink color:** changed from `LCD_COLOR_GREEN_DARK` (0x0300) to standard `LCD_COLOR_GREEN` (0x07E0). Dark green was too dim on the LCD.

### Flash / test procedure for Optimization 1

1. Build: `. ~/esp/esp-idf/export.sh && idf.py -D BOARD=waveshare-esp32-s3-lcd-147 build`
   (first build downloads esp-dsp from component registry)
2. SCP binaries to Pi: `scp build/bootloader/bootloader.bin build/partition_table/partition-table.bin build/weldml-esp32.bin casey@192.168.1.43:/tmp/`
3. User: hold Key1, press Key2 (download mode)
4. Flash: `ssh casey@192.168.1.43 "python3 -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 --before no-reset --after hard-reset --no-stub write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0 /tmp/bootloader.bin 0x8000 /tmp/partition-table.bin 0x10000 /tmp/weldml-esp32.bin"`
5. User: press Key2 to boot
6. Copy l060.fsj: `ssh casey@192.168.1.43 "sudo mount /dev/sda1 /tmp/sdmount && sudo cp /tmp/l060.fsj /tmp/sdmount/ && sudo sync && sudo umount /tmp/sdmount"`
7. Wait for blink, then read result: `ssh casey@192.168.1.43 "sudo mount -o ro /dev/sda1 /tmp/sdmount && cat /tmp/sdmount/weldml_result.json && sudo umount /tmp/sdmount"`

---

## MVP Closure — LCD Writing-State Color Fix (2026-07-23)

**Change:** `WELD_STATE_WRITING` color changed from `LCD_COLOR_YELLOW` (0xFFE0) to
`LCD_COLOR_WHITE` (0xFFFF) in `components/weld_processor/weld_processor.c` for stronger
contrast against the `LCD_COLOR_GREEN` (0x07E0) result blink. WS2812B RGB LED (GPIO38)
formally descoped for MVP — wired but never driven by firmware; see `MVP_REQUIREMENTS.md`.

**Hardware test (Waveshare ESP32-S3-LCD-1.47, Pi workbench SLOT3, firmware `c2c1f3c`):**

| Test | Result | Notes |
|------|--------|-------|
| Build (`idf.py -D BOARD=waveshare-esp32-s3-lcd-147 build`) | PASS | Zero errors/warnings |
| Flash via Pi SSH esptool (Key1+Key2 download mode, Key2 to boot) | PASS | SHA hash verified |
| Full color-state cycle, triggered twice via `l060.fsj` copy to SD | PASS | CYAN (idle) → WHITE (writing) → BLUE (processing) → GREEN blink (NP result) → CYAN |
| White/green contrast vs. prior yellow/green | PASS — user confirmed visually | "works" |

MVP declared complete by user after this test. Remaining `MVP_REQUIREMENTS.md` out-of-scope
items (OTA, MQTT, WiFi, web server, BLE, multi-file batch, WS2812B LED, etc.) are
intentionally unimplemented — not gaps.

---

## MVP Feature Addition — Centered Status Text on LCD (2026-07-23)

**Change:** Added `lcd_st7789_draw_text_centered()` (built-in 5x7 bitmap font, uppercase
A-Z subset needed for status words only) to `components/lcd_st7789/`. Each color-fill
state in `weld_processor.c` now also draws its label (READY/WRITING/PROCESS/PASS/FAIL)
in black over the full-screen color. Text is rendered rotated 90° to run along the
panel's 320px-tall axis instead of its 172px-wide axis, at 7x scale (largest integer
scale that fits the 7-character labels WRITING/PROCESS within 320px).

**Hardware test (Waveshare ESP32-S3-LCD-1.47, Pi workbench SLOT3, firmware `30591e6`):**

| Test | Result | Notes |
|------|--------|-------|
| Build (`idf.py -D BOARD=waveshare-esp32-s3-lcd-147 build`) | PASS | Zero errors/warnings |
| Flash via Pi SSH esptool (Key1+Key2 download mode, Key2 to boot) | PASS | SHA hash verified |
| READY label on idle (cyan) screen | PASS — user confirmed | Rotated text renders correctly oriented (not mirrored/upside-down) and legible |
| WRITING label (white), triggered by copying `l060.fsj` to SD via Pi mount/cp/sync/umount | PASS — user confirmed | Two full cycles run; label visible during the write-idle window |
| PROCESS label (blue), same trigger | PASS — user confirmed | Visible during `process_fsj_file()` before the result blink |
| PASS label (green blink), same trigger | PASS — user confirmed | `l060.fsj` is the LOOCV/NP fixture — expected PASS |

| FAIL label (red blink), triggered by copying `l046.fsj` (LOOCV/IF fixture) to SD | PASS — user confirmed | Correctly predicted IF; red blink with FAIL text confirmed |

All five status labels (READY/WRITING/PROCESS/PASS/FAIL) are now independently
hardware-verified.

---

## Milestone 2 Ticket #3 — WiFi Bring-up + Reachable Web UI (2026-08-06)

**Change:** New `components/wifi_provision/` (station mode with NVS-stored credentials,
SoftAP fallback on no-credentials or failed/timed-out connect). Partition table changed
to the Q20-resolved layout (`otadata`+`ota_0`+`ota_1`+`spiffs`, replacing the old single
`factory` partition). `main.c` now calls `nvs_flash_init()`/`esp_netif_init()`/
`esp_event_loop_create_default()`/`wifi_provision_start()`/`webserver_start()`. Added
`webserver_register_uri()` extensibility hook (for #4/#6's future endpoints) and
`spiffs_create_partition_image()` in the root `CMakeLists.txt` — previously missing
entirely in this repo, so the `spiffs` partition was blank flash and every static-file
request 404'd even with the httpd server running correctly.

**Hardware test (Waveshare ESP32-S3-LCD-1.47, Pi workbench SLOT3, via the workbench's
WiFi-testing portal at `192.168.1.43:8080` — `workbench.local` did not resolve, used the
IP directly):**

| Test | Result | Notes |
|------|--------|-------|
| Build (`idf.py -D BOARD=waveshare-esp32-s3-lcd-147 build`), full clean rebuild | PASS | Zero errors/warnings |
| Flash (bootloader+partition-table+ota_data_initial+app @ new offsets: 0x0/0x8000/0xf000/0x20000) | PASS | SHA hash verified. Key2 press required after flash to boot — same known quirk as prior sessions (hard-reset via RTS doesn't exit download mode on this board) |
| SoftAP fallback on fresh flash (no NVS credentials, empty Kconfig defaults) | PASS | `ESP32-Setup` (open, ch1) confirmed via workbench WiFi scan |
| Web UI reachable in SoftAP mode | PASS — then FAILED then PASS | First attempt: `GET /` returned 404 despite `/api/status` returning 200 — root cause: `spiffs_create_partition_image()` was never wired into the build, so the spiffs partition was blank and auto-formatted empty. Fixed by adding it to root `CMakeLists.txt`, rebuilt, reflashed just the `spiffs` partition (0x620000) — `GET /` then returned the real `index.html` (WiFi Setup page), verified by decoding the response body |
| WiFi provisioning via web UI (`POST /api/wifi`) | PASS | Posted test AP credentials (`WeldMLTest`) while joined to the device's SoftAP; got `{"ok":true,"message":"Saved. Rebooting."}` |
| Station-mode connect using saved NVS credentials | PASS | Workbench `STA_CONNECT` event fired with the device's real MAC (`98:3d:ae:e4:4e:ac`), assigned IP `192.168.4.15` |
| Web UI reachable in station mode | PASS | `GET /api/status` on the station IP returned `wifi_ssid: "WeldMLTest"`, correct RSSI |
| Factory reset (`POST /api/factory-reset`) returns device to clean state | PASS | NVS erased, rebooted, `ESP32-Setup` SoftAP confirmed broadcasting again via a final scan |
| SoftAP fallback on *failed* station connect (not just no-credentials) | PASS | Posted credentials for a nonexistent SSID (`NoSuchNetwork-XYZ`); after the ~15s boot-time connect/retry window, `ESP32-Setup` reappeared in a scan, confirming the retry-then-fallback branch (`STA_MAX_BOOT_RETRY` exhausted → `WIFI_FAIL_BIT` → `esp_wifi_stop()` → SoftAP), not just the empty-credentials branch |

**Not verified this session:** steady-state reconnect-after-drop behavior (the
`wifi_event_handler`'s indefinite-retry path once past the boot-time decision window) —
the design change from the base template's original "logs an error but does not fall
back" gap (see *Known Gaps* below, now stale) was exercised only for the *initial*
boot-time connect/fallback decision, not a mid-session AP drop-and-recover. CPU
contention between WiFi/webserver and the weld-processing pipeline (Q21) also remains
unverified — no `.fsj` file was processed with WiFi/webserver active during this session.

---

## Milestone 2 Ticket #4 — Results Cache + `GET /api/results` (2026-08-06)

**Change:** `weld_processor` now maintains a mutex-guarded, 50-entry in-memory results
cache (`weld_cloud_cache_append()`), appended to at the same point a row is written to
`weldml_results.csv` — never repopulated by re-reading the CSV (Q23). A new `GET
/api/results` endpoint, registered via #3's `webserver_register_uri()` hook, serves the
cache as JSON (`weld_cloud_build_results_json()`) on the HTTP server's own task, mutex-
guarded against the monitor task's concurrent writes. Also extends `weldml_results.csv`
and the cache from 2 to all 22 extracted features (Q22's acceptance-criteria addition on
issue #4) — `write_result_json()` and `write_error_json()` now share a single
`write_csv_header()` so the header and every row always agree on the 33-column shape.

**Hardware test (Waveshare ESP32-S3-LCD-1.47, Pi workbench SLOT3, via the workbench
WiFi-testing portal at `192.168.1.43:8080`):**

| Test | Result | Notes |
|------|--------|-------|
| Build (`idf.py -D BOARD=waveshare-esp32-s3-lcd-147 build`), full clean rebuild | PASS | Zero errors/warnings |
| Flash (all 5 binaries incl. spiffs) | PASS | SHA hash verified; Key2 press required to boot (same known quirk) |
| `GET /api/results` on fresh boot (empty cache) | PASS | Returned `[]` |
| Real weld cycle (`l060.fsj`, LOOCV/NP fixture, copied to SD via Pi mount/cp/sync/umount) appears via `GET /api/results` **without touching the LCD** | PASS | `predicted_class:0`, `label:"NP"`, `FFT_FrequencyBandwidth:33.1813202`, `MinPositionStage3:1.86000001`, `window_count:1854` — exact match to this fixture's previously-verified values (see the 2026-07-23 Performance Investigation section above). All 22 features present in the JSON, not just 2 |
| `weldml_results.csv` on the SD card reflects the same 22-feature row, 33 columns matching the new header shape | PASS | Verified via `cat` over SSH after read-only mount. (The file's own header line is stale — 11/13 columns, written once long ago when the file was first created under an older schema; this append-only file never rewrites its header. Not a regression — a future Clear/truncate, per Q14, will produce a fresh correctly-shaped header.) |
| Cache accumulates multiple rows correctly (`l046.fsj`, LOOCV/IF fixture, copied second) | PASS | `GET /api/results` returned 2 rows, oldest-first: `l060.fsj` (NP) then `l046.fsj` (IF), each with correct `predicted_class`/`label` |

**Not verified this session:** cache eviction past the 50-entry capacity (only 2 rows were
ever cached); the error-row CSV shape (`write_error_json()`'s 33-column padding) — reasoned
about and independently verified via a Python field-count simulation before writing the C,
but no error condition was triggered on real hardware to confirm it end-to-end.

---

## Configurable Server URL + Multi-WiFi Saved-Network List (2026-08-08)

**Change:** `weld_processor.c`'s upload handler now reads a `tb_url` NVS field (new
`/config` page input) instead of the fixed `#define THINGSBOARD_HOST`, defaulting to
`iot.mwe-inc.com` when unset — lets the target server/platform be changed or tested
without a firmware rebuild. `components/wifi_provision/` reworked from a single
`wifi_ssid`/`wifi_pass` NVS pair to an ordered list (`wifi_count` + `wifi_ssidN`/
`wifi_passN`, index 0 = most recently added, capped at `WIFI_PROVISION_MAX_NETWORKS`=5);
adding a network promotes it to the front instead of overwriting, both the initial boot
attempt and every background-retry cycle (while in AP fallback) now try the whole list
in order rather than a single network. One-time migration (`wifi_list_migrate_legacy()`)
carries an existing single saved network into the new list format transparently. New
`GET /api/wifi/list` / `POST /api/wifi/delete` endpoints back a "Saved Networks" section
on the WiFi page with per-entry delete buttons. Results page button relabeled "Upload to
Server" (was "Upload to ThingsBoard") to match the now-configurable server.

**Hardware test (Waveshare ESP32-S3-LCD-1.47, Pi workbench SLOT3, manual Key1+Key2
download mode + Pi SSH esptool, same as prior sessions):**

| Test | Result | Notes |
|------|--------|-------|
| Build (`idf.py -D BOARD=waveshare-esp32-s3-lcd-147 build`), full clean rebuild | PASS | Zero errors/warnings |
| Host test suite (`weld_cloud`, `weld_inference`) | PASS | Unaffected by this session's changes (hardware-glue only). `test_weld_parser_features` still fails on the pre-existing, unrelated issue #7 |
| Flash (all 5 binaries incl. spiffs) | PASS | Both firmware and spiffs hashes verified; Key2 press required to boot (same known quirk) |
| Legacy single-credential migration to the new list format, on first boot after this flash | PASS | `GET /api/wifi/list` returned `["Other"]` — the credential entered in a previous step of this same session carried forward with no user action and no re-provisioning needed |
| Station reconnect using the migrated credential | PASS | Device came back up on `192.168.1.61` (`/api/status` showed `wifi_ssid:"Other"`, `uptime_ms` consistent with the fresh boot) |
| `/config` page serves the new Server URL field | PASS | `GET /config` HTML contains `id="tb_url"` and the "Server URL"/"Server Access Token" labels |
| `/results` page shows the relabeled button | PASS | `GET /results` HTML contains "Upload to Server", not "Upload to ThingsBoard" |
| `/` page serves the new Saved Networks section | PASS | `GET /` HTML contains the "Saved Networks" heading and list container |
| `POST /api/wifi/delete` negative case (nonexistent SSID) | PASS | Returned `{"ok":false,"error":"not found"}` without touching the device's only live saved network |

**Not verified this session (deliberately — no second real network was available to test
with safely):** the actual most-recently-added-tried-first ordering with two or more real
saved networks; deleting a network that's actually in the list; the full-list background
retry cycling while already in AP fallback. The 2-minute steady-state SoftAP-fallback
timing test (flagged unverified in the previous handoff) also remains untested.

---

## Ticket #5 — Upload Stack-Overflow Crash Fix + ThingsBoard Widget Time Window (2026-08-08)

**Bug found on hardware:** `POST /api/upload` (ticket #5) crashed the device every time it
actually had a row to send — this was the first time the firmware had ever made a real
outbound HTTPS/TLS call on real hardware, and it had never been exercised end-to-end before.
Root cause: `handler_api_upload()` runs `esp_http_client` + `esp_crt_bundle_attach` (a full TLS
handshake and cert-bundle parse) synchronously inside an ESP-IDF httpd worker-task callback,
whose default stack (`HTTPD_DEFAULT_CONFIG()`, `esp_http_server.h`) is only 4096 bytes —
too small for mbedTLS's handshake/cert-bundle stack usage, causing a stack-overflow reboot
partway through the call. Confirmed via ThingsBoard's own telemetry (checked directly via the
MCP connection): no payload data had ever reached the platform from this device before the fix,
consistent with the crash happening mid-request. **Fix:** `webserver_start()` now sets
`config.stack_size = 8192` (matching what `components/ota/ota.c` already uses for the same kind
of TLS call) in `components/webserver/webserver.c`.

**Hardware test (Waveshare ESP32-S3-LCD-1.47, Pi workbench SLOT3):**

| Test | Result | Notes |
|------|--------|-------|
| Build + flash with the stack-size fix | PASS | Zero errors/warnings; SHA-verified flash |
| `POST /api/upload` with a real cached row (`l060.fsj`) | PASS | `{"ok":true,"uploaded":1}`, no crash, `uptime_ms` climbed normally afterward (confirmed no reboot) |
| Data integrity on the platform side | PASS | Verified directly via ThingsBoard's own telemetry (MCP `getLatestTimeseries`) — `label:"NP"`, `FFT_FrequencyBandwidth:33.1813202`, `MinPositionStage3:1.86000001`, `window_count:1854`, `source_filename:"l060.fsj"` all landed exactly matching the fixture |

**Side quest — the PASS/FAIL dashboard widget also needed a time-window fix**, unrelated to the
firmware bug: `weld_cloud_build_payload()` timestamps uploads using the weld's own embedded
timestamp (not upload time — a deliberate earlier design choice), so `l060.fsj`'s 2020-07-17
timestamp was invisible to the widget's "Current day" window even after the upload succeeded.
Corrected to **History → Last → 2700 days** (not "Range"/fixed dates — confirmed against
[ThingsBoard's own docs](https://thingsboard.io/docs/user-guide/time-window/) that a fixed
range does not auto-advance and would silently go stale). Verified live: widget now shows
PASS:7 / FAIL:4 / Total:11. See `docs/THINGSBOARD_SETUP.md` Section 5 for the corrected
step-by-step.

**Not verified this session:** whether the crash's brief but severe USB-enumeration disruption
(observed during debugging — the board's native USB dropped off the Pi's hub hard enough to
need a full hub power-cycle to recover once) ever recurs under normal (non-crashing) upload
use now that the fix is in; the fixed httpd worker stack size's effect on overall free-heap
margin under sustained/repeated use (single-call heap delta observed was small, ~25KB, not
flagged as a leak but not stress-tested either).

---

## 2-Minute SoftAP-Fallback Timing Test (2026-08-08)

**What was tested:** the steady-state outage → SoftAP fallback behavior added in the
`dceb48f` `wifi_provision` rewrite (2-minute threshold before an already-connected device
brings up its SoftAP, background retry cycling the whole saved-network list every 30s
afterward) — flagged unverified in every handoff since it was written, because it requires
real wall-clock waiting.

**Method:** rather than disrupt the user's real home WiFi router, added the workbench Pi's
own test AP (`wb-fallback-test`) as a saved network via `POST /api/wifi` (promotes to front,
tried first) — device rebooted and connected to it at `192.168.4.15`, confirmed via
`GET /api/status`. Stopped the workbench AP (`POST /api/wifi/ap_stop`) to simulate an outage
of the currently-active network without touching the user's own network at all, then polled
`GET /api/wifi/scan` on the workbench every 8s watching for `weldml-esp32_1` to appear/disappear.

| Test | Result | Notes |
|------|--------|-------|
| No premature SoftAP during the "quick retry" phase | PASS | Nothing appeared for the first ~90s of the outage |
| SoftAP appears after the 2-minute threshold | PASS (timing approximate) | Observed at outage+~97-124s depending on measurement start-time uncertainty (a few seconds of tool-call latency between issuing the outage and arming the poll watcher) — consistent with the intended 120000ms threshold, not pinned to the exact millisecond. The device's own `ESP_LOGW("Disconnected for %lld ms...")` would give the precise value but wasn't captured live this run |
| Background retry cycles the *whole* saved-network list (this session's new behavior), not just the last-tried network | PASS | Device recovered onto `Other` (the second saved entry) without ever being told about it directly during the outage — confirms the `try_all_networks()`-in-monitor-task change actually works on hardware, not just in the boot-time path |
| SoftAP drops once reconnected | PASS | Disappeared at outage+177s; the ~53s gap after the 2-minute mark is consistent with one failed 30s retry cycle (still trying the dead `wb-fallback-test` first each cycle) before a second cycle reached `Other` and succeeded |
| Device stayed up throughout, no reboot | PASS | `uptime_ms` climbed continuously across the whole test (263564ms at the end) |

**Cleanup:** removed `wb-fallback-test` from the saved list (`POST /api/wifi/delete`) and
stopped the workbench AP afterward — device's saved list is back to `["Other"]` only, matching
normal operation.

**Not verified this session:** the exact millisecond precision of the 2-minute threshold (see
above); behavior when *no* fallback network exists at all (list would just stay empty after
deletion and the device would sit in AP-fallback forever, never verified live); repeated
outage/recovery cycles back-to-back (only one full cycle was exercised).

---

## Ticket #6 — Clear Button (2026-08-08)

**Change:** New `POST /api/clear` in `weld_processor.c`, gated by `weld_cloud_check_clear_allowed()`
(pure logic, TDD'd in an earlier session) against the same global-watermark/eviction-offset
conversion `handler_api_upload()` already uses. The HTTP handler only ever sets a pending flag
(`s_clear_pending`) — never opens the SD card itself. `monitor_task`'s write-idle loop now also
checks that flag in both branches (when idle with no write activity at all, and right after
`process_sd()` finishes), running the actual work (`process_clear()`: truncate
`weldml_results.csv` to header-only, empty the in-memory cache, reset the NVS upload watermark
to 0) inside the same `tinyusb_msc_storage_mount()`/`tinyusb_msc_storage_unmount()` bracket
`process_sd()` already uses for its own SD access — no new SD ownership state. `results.html`
got a Clear button; when unsent rows are reported, a warning + "Clear Anyway" override button
appears.

**Hardware test (Waveshare ESP32-S3-LCD-1.47, Pi workbench SLOT3):**

| Test | Result | Notes |
|------|--------|-------|
| Build + host tests (`weld_cloud`) | PASS | Zero warnings; host tests unaffected (hardware-glue only, no changes to the already-tested pure-logic function) |
| Refused path (unsent rows, no override) | PASS | `POST /api/clear` → `{"ok":false,"error":"unsent rows present","unsent":1}`; confirmed both the cache and `weldml_results.csv` on the SD card completely unchanged (22 lines, all prior rows intact) |
| Normal path (all sent, no override) | PASS | Succeeded, CSV truncated to exactly 1 line (header-only). Watermark reset verified with a *discriminating* test, not just an empty cache: added one more fresh row, uploaded it, got `{"ok":true,"uploaded":1}` — only possible if the watermark genuinely reset to 0 (a stale un-reset watermark would have wrongly suppressed this identical-count case as already-sent) |
| Override path (unsent rows, force:true) | PASS | Cleared anyway; CSV truncated, cache emptied |
| `.fsj` source files never touched | PASS | `l046.fsj`/`l060.fsj`/`l314.fsj` confirmed present and unchanged throughout every scenario |
| Web UI (real browser, not just API) | PASS | User confirmed the warning + "Clear Anyway" button appear correctly for the unsent-rows case, and independently exercised the Upload-then-Clear normal path themselves through the UI |

**Not verified this session:** concurrent Clear-while-actively-writing (i.e. requesting Clear
during the ~5s `IDLE_WINDOW_MS` debounce right after a real MSC write, before `process_sd()`
runs) — the code path exists (checked again right after `process_sd()` in the same window) but
was never exercised with real timing on hardware; repeated rapid Clear requests back-to-back.

---

## Milestone 3 Ticket #9 — CPU-Contention Protection, Hardware Re-Verification (2026-08-08)

**Change:** `weld_processor.c` brackets the write-idle SD-ownership window with
`webserver_stop()`/`webserver_start()` (zero WiFi/HTTP task activity during processing;
re-registers weld_processor's own endpoints on the way back out, since `webserver_stop()`
also tears down SPIFFS). Defense in depth: `weld_mon` raised from priority 5 to 10 and
pinned to core 1 (matches `CONFIG_TINYUSB_TASK_AFFINITY_CPU1`; WiFi's internal task is
pinned to core 0). A mid-flight `POST /api/upload` is now waited out (bounded to 16s)
before `webserver_stop()` is called, since `httpd_stop()` itself blocks its caller until
the single synchronous httpd task is free — found during code review, not part of the
original ticket text. Resolves `docs/OPEN_QUESTIONS.md` Q21.

**Hardware test (Waveshare ESP32-S3-LCD-1.47, Pi workbench SLOT3, firmware `e40337c`,
WiFi connected to `Other`, webserver active throughout):**

| Fixture | Run | `parse_ms` | `features_ms` | Total | Classification |
|---|---|---|---|---|---|
| `l060.fsj` (LOOCV/NP) | 1 | 1,773 | 3,903 | 5,676 ms | NP ✓ |
| `l060.fsj` (LOOCV/NP) | 2 | 1,780 | 3,885 | 5,665 ms | NP ✓ |
| `l046.fsj` (LOOCV/IF) | 1 | 874 | 2,044 | 2,918 ms | IF ✓ |
| `l046.fsj` (LOOCV/IF) | 2 | 878 | 2,044 | 2,922 ms | IF ✓ |

Two runs per fixture happened because an identically-named `.fsj` was already left on the
SD card from earlier testing — mounting the card to copy the new file triggered one
reprocessing pass of the stale file, then the `cp` overwrite triggered a second, genuine
pass. Not a bug; both runs are independent, valid measurements.

**Result:** No regression against the 2026-07-23 baseline (`l060.fsj`: `parse_ms` 2,004 ms,
`features_ms` 3,859 ms, total ~5.9 s) — total time for `l060.fsj` was actually slightly
*lower* under this change (~5.67 s), consistent with the webserver being fully stopped
(rather than time-sliced against equal-priority WiFi/HTTP tasks) during the window. Four
processing cycles ran across ~130 s of continuous uptime (`uptime_ms` 63,007 → 193,611,
monotonic, no gaps) with zero Task Watchdog resets and zero crashes — the priority-10/
core-1 defense-in-depth layer flagged as a WDT-starvation risk during code review did not
manifest; `features_ms` stayed well under the 5 s `CONFIG_ESP_TASK_WDT_TIMEOUT_S` window
each time. `GET /api/results` correctly returned all 4 cached rows afterward, confirming
`webserver_start()` + endpoint re-registration succeeded on every one of the 4 window
exits, not just the first.

**Not verified this session:** the mid-flight-upload wait added during code review
(`s_upload_in_progress`) — never exercised on hardware, since no `POST /api/upload` was
in flight at the moment a write-idle window elapsed during this test. The logic is
straightforward (bounded poll loop) but the actual race condition it defends against
was not reproduced.

---

## Milestone 3 Ticket #10 — Real OTA Download + Checksum Gate + Rollback (2026-08-08)

**Change:** `ota.c` fetches `fw_title`/`fw_version`/`fw_checksum`/`fw_checksum_algorithm`
fresh from ThingsBoard at trigger time, streams the firmware into the inactive OTA partition
while accumulating a SHA-256 hash of the exact bytes written, and only calls
`esp_ota_set_boot_partition()` if that hash matches the advertised checksum and
`esp_ota_end()`'s own image-structure validation both pass — any mismatch calls
`esp_ota_abort()` instead, per `docs/OPEN_QUESTIONS.md` Q19. `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`
enabled; `main.c` calls `esp_ota_mark_app_valid_cancel_rollback()` at the existing
LCD+SD-init-succeeded gate, before `weld_processor_start()`. Web UI (`/ota`) polls
`/api/status` for live Downloading…/Verifying…/Success — rebooting/Failed states.

**Hardware test (Waveshare ESP32-S3-LCD-1.47, Pi workbench SLOT3):**

| Test | Result | Notes |
|---|---|---|
| Build (`idf.py -D BOARD=waveshare-esp32-s3-lcd-147 build`) | PASS | Zero errors/warnings |
| Host tests (`weld_cloud`/`weld_inference`/`ota_policy`, 9 new checksum-verify assertions) | PASS | `test_weld_parser_features` still fails on the pre-existing, unrelated issue #7 |
| Real OTA package created via ThingsBoard's own admin UI (not the MCP upload tool — still blocked by the same host-filesystem `filePath` limitation as the ticket #8 session) | PASS | `weldml-esp32`/`ota-test-1`, SHA256, assigned to `weldml-esp32-lab-01` via its device-profile-inherited "Assigned firmware" field |
| First real OTA attempt | FAIL (expected — real bug found) | `fw_checksumAlgorithm` (camelCase) used instead of the already-correctly-specified `fw_checksum_algorithm` (snake_case) in Q19/Q17 — algorithm came back empty, `ota_policy_verify_checksum()` correctly failed closed rather than trust an unverifiable checksum. Confirmed via the checksum ThingsBoard's UI displayed matching the locally-computed SHA256 of the exact same binary — proved the download/hash pipeline was correct and the bug was purely the attribute key name. Fixed, commit `9771a7f`. |
| Real OTA attempt after fix (×2) | PASS | Full cycle both times: fetched real attributes, downloaded the real package, computed SHA-256 matched ThingsBoard's advertised checksum, `esp_ota_end()` validated, boot partition switched, rebooted. Confirmed via `/api/status`'s version field changing and `uptime_ms` resetting (genuine reboot, not a no-op) |
| Live web UI status (real browser, not just JSON) | PASS — user confirmed | Second post-fix attempt: user watched `/ota` show "Downloading…" → "Verifying…" → "Success — rebooting" in sequence, matching all four required states. (First post-fix attempt's UI wasn't clearly observed by the user — likely just missed given the page wasn't watched continuously; the backend state transitions were independently confirmed via polling both times regardless) |
| Manual-trigger-only (no automatic/unattended update) | PASS by construction | Only reachable via `POST /api/ota`, itself only called from the web UI button click |

**Not verified this session:** the mid-flight-weld-write guard (`g_weld_write_active` check in
`run_update()`, added during code review per the CLAUDE.md flash-safety rule) — never exercised
against a real concurrent USB-MSC write, since no weld cycle was triggered during any OTA
attempt this session. Also not tested: rollback itself (a broken image actually failing to
confirm and the bootloader auto-reverting) — that is ticket #11's explicit, separate job.

**Testing artifact, not a product concern:** the `ota-test-1` package's binary content is the
*original pre-fix* build (uploaded before the `fw_checksum_algorithm` bug was found), so every
time OTA succeeds against it, the device lands back on that old binary — which will then fail
the *next* OTA attempt again (same already-fixed bug, just re-encountered because the package
itself was never re-uploaded). This caused real confusion mid-session (repeated flash cycles
to get back to a clean state) but only affects this specific stale test package; a real release
build will never have this problem since the fix ships as part of it. Device was left on the
fixed build (`9771a7f`) on both OTA partitions at the end of this session, deliberately, so a
future OTA attempt won't immediately re-trigger this already-fixed bug.

---

## Milestone 3 Ticket #11 — Rollback Test — CONFIRMED via two clean, repeatable single-reset trials (2026-08-12)

**Status:** Originally closed on the strength of one confounded session (see "Original session"
below), then reopened after review showed that close overstated the evidence — only one of that
session's trials was actually clean. A same-day retest fixed this: two independent trials, each a
fresh broken-image OTA push followed by exactly **one** Key2 press, both immediately confirmed via
LCD redraw and `/api/ota/check`, with no ambiguity and no confound. See "Retest" below for the
clean result; the "Original session" table is kept for the record but should not be cited alone.

**Retest (2026-08-12, same day, after reopening):** Rebuilt the same deliberate-hang broken image
on a fresh throwaway branch (`test/ticket-11-retest-broken-ota`, never merged), uploaded as a new
distinct ThingsBoard package (`ota-test-broken-11-retest`) per the same never-overwrite convention.
Two trials, both against the live device:

| Trial | Reset | LCD result | `/api/status` uptime | `/api/ota/check` result |
|---|---|---|---|---|
| 1 | 1× Key2 | Fresh "Ready" redraw (was frozen/dark before reset) | 10584 ms | `current: "11f4a37-dirty"`, not the broken version |
| 2 | 1× Key2 | Fresh "Ready" redraw | 9318 ms | `current: "11f4a37-dirty"`, not the broken version |

Both trials: single reset, immediate LCD redraw (impossible for the broken image, which never
touches the LCD), immediate WiFi/HTTP confirmation — no unplug/replug needed either time, unlike
the original session. **This is the clean, repeatable confirmation the reopened issue asked for.**
`ota_1` was reflashed back to the good build afterward (hash-verified) to restore the standard
both-partitions-good end-of-ticket state — this step was interrupted once by an unrelated Pi
workbench network outage (Pi became fully unreachable mid-flash, unrelated to anything ESP32-side;
resolved by the user power-cycling the workbench, board came back up cleanly on its own with no
data loss since the interrupted operation was the cosmetic `ota_1` reflash, not anything touching
`ota_0` or the rollback result itself) and once by the board landing back in USB download-mode
after a flash's auto-reset didn't take (cleared by one plain Key2 press, no data loss).

**Original session (2026-08-12, earlier the same day) — kept for the record, cite the retest above
instead:** The table below reads as a clean multi-step PASS, but the underlying evidence was
thinner than that implied: the first OTA-into-broken-image attempt (2× Key2 + 1 Pi power-cycle)
stayed dark through all three resets and was never actually explained — it was attributed to the
DTR/RTS serial-port confound below, but that attribution was never verified against what was
actually done during those three resets, and "rollback doesn't trigger on the first reset" was not
ruled out. Only the second attempt, a single Key2 press producing a "Ready" LCD screen plus a later
`/api/ota/check` confirmation, was a clean, unconfounded data point — and it was exactly one trial.

**Purpose:** Ticket #10's session only ever exercised genuinely-valid OTA images. Ticket #11
exists to prove the ESP-IDF bootloader's app-rollback feature actually auto-reverts to the
previous working partition when a bad image never reaches
`esp_ota_mark_app_valid_cancel_rollback()` — never tested before this session.

**Real latent bug found and fixed first:** the local, gitignored `sdkconfig` had an explicit
`# CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE is not set` line predating commit `8ce9182` (the commit
that added `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` to `sdkconfig.defaults`). ESP-IDF's defaults
mechanism never overrides an already-explicit line, even a "not set" one — so the bootloader
ticket #10's session believed it had enabled and flashed almost certainly never actually had
rollback compiled in, on any build since 2026-08-07, undetected across two more handoffs because
that session never exercised the failure path. Fixed by removing the stale lines and running
`idf.py reconfigure -D BOARD=waveshare-esp32-s3-lcd-147`; confirmed via
`build/bootloader/config/sdkconfig.h` showing `#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1`
(the actual ground truth for what a bootloader *build* contains — the app-level sdkconfig.h does
not govern bootloader behavior). Full 5-binary reflash (bootloader + partition-table + otadata +
app + spiffs) via manual download mode + SSH `esptool` to the Pi, all writes hash-verified.

**Hardware test (Waveshare ESP32-S3-LCD-1.47, Pi workbench SLOT3):**

| Test | Result | Notes |
|---|---|---|
| Broken image built: `while(1)` loop + one `ESP_LOGE` at the top of `app_main()`, before `nvs_flash_init()`, before the rollback-confirmation gate | N/A (deliberate) | Never committed to `main`; built on a throwaway branch as an uncommitted working-tree edit only |
| Real OTA package created on ThingsBoard (`weldml-esp32`/`ota-test-broken-11`, distinct from `ota-test-1`, uploaded via the admin UI due to the same MCP host-filesystem `filePath` limitation as ticket #10) | PASS | Assigned to `weldml-esp32-lab-01`; checksum verified matching the local build |
| OTA push against the fixed (genuine rollback-enabled) bootloader — attempt 1 | INCONCLUSIVE | 2× Key2 + 1 Pi power-cycle, device stayed dark/silent through all three. Presumed caused by the DTR/RTS serial-port confound below, but that was never actually verified — do not treat this attempt as evidence either for or against rollback |
| OTA push, redone — attempt 2, single Key2 reset | PASS (one trial) | Screen showed "Ready" in normal coloring — structurally impossible for the broken image, which never touches the LCD — real evidence rollback engaged, but only one clean trial so far |
| `/api/ota/check` after WiFi recovered (attempt 2) | PASS (one trial) | `current: "11f4a37-dirty"`, not `ota-test-broken-11` — confirms the version reverted for this one trial; not yet repeated |
| Full functionality after revert | PASS | WiFi reconnected, `/`, `/ota`, `/results` all HTTP 200, `/api/results` responding normally |
| Manual USB reflash (last-resort recovery) | PASS | Exercised for real mid-session as the actual recovery path for the bootloader fix above, not just tested in isolation |

**A confound worth recording for future sessions on this board:** this workbench's RFC2217 proxy
(`/usr/local/bin/plain_rfc2217_server.py`) passes DTR/RTS straight through to the physical chip
(a documented ESP32-S3-native-USB workaround). Opening the serial port at all — even passive
monitoring or `esptool`'s own connect handshake — can silently knock the chip into ROM
download/bootloader mode as a side effect, independent of whatever firmware is flashed. This
contaminated several early reset/observation cycles this session before being identified.
**Workaround: never open this board's serial port for passive observation; use physical
Key1/Key2 presses for reset and check state only via the device's own HTTP endpoints or the LCD.**

**Not yet root-caused — a genuine new finding:** WiFi does not reliably reconnect after an
EN-pin-only reset (Key2), even though the app itself boots correctly (LCD reaches "Ready" every
time). Observed twice this session — once after the rollback-proving Key2 press, once after a
routine post-cleanup reflash — in both cases a full USB unplug/replug from the Pi hub fixed it
immediately, while a Key2 press alone sometimes did not. Not the same issue as the DTR/RTS serial
confound above; this is the application's WiFi stack, not the bootloader-select path. See
`docs/OPEN_QUESTIONS.md` Q25 (open).

**Cleanup:** device left with the fixed build (`11f4a37-dirty` or later) on both OTA partitions,
confirmed via a full 5-binary bootloader reflash from this session's original attempt plus a
directly-verified `ota_1` app-partition reflash after the retest. `weldml-esp32-ticket11-broken.bin`,
`weldml-esp32-ticket11-retest-broken.bin`, and both throwaway branches
(`test/ticket-11-broken-ota`, `test/ticket-11-retest-broken-ota`) deleted from the local repo. Both
ThingsBoard package assignments used this session (`ota-test-broken-11`, `ota-test-broken-11-retest`)
were deliberately left in place on `weldml-esp32-lab-01` (harmless, manual-trigger-only — same
treatment as `ota-test-1` after ticket #10).

**Retest bonus finding for Q25:** both retest trials' WiFi came back within ~10s of the single Key2
press, with no unplug/replug needed — unlike the original session's two occurrences. Not enough to
call Q25 resolved (2 clean vs. 2 flaky is still a coin flip on this evidence), but worth recording:
whatever's flaky about post-EN-reset WiFi reconnect, it isn't 100% reproducible on demand.

---

## Deferred to Product Fork

These items are not part of the base template and do not need to be tested here:

| Item | Reason deferred |
|------|----------------|
| MQTT publish/subscribe | Template has no auto-publish. Product fork calls `mqtt_publish()` with application data. |
| OTA trigger via MQTT | Requires product-specific MQTT topic handler. Add in brand/product fork. |

---

## Known Gaps / Future Work

- **WiFi re-provisioning:** No UI to clear saved credentials and re-enter setup mode
  without manually erasing NVS. A "forget WiFi" button on the config page would be
  useful in a product fork.

- ~~**STA connection failure fallback**~~ — **Resolved 2026-08-06** by
  `components/wifi_provision/`: a failed/timed-out boot-time station connect now falls
  back to SoftAP (hardware-verified — see Milestone 2 Ticket #3 above). Steady-state
  reconnect-after-drop (once already connected) still just retries station mode
  indefinitely rather than falling back to SoftAP mid-session — not yet hardware-verified.

- ~~**SPIFFS web files not auto-built on first clone**~~ — this claim was never true for
  this repo: `spiffs_create_partition_image()` was missing from the root `CMakeLists.txt`
  entirely until 2026-08-06 (Milestone 2 Ticket #3), so the `spiffs` partition was blank
  flash and every static-file request 404'd. Now fixed — the SPIFFS image is generated
  by `idf.py build` and flashed by `idf.py flash`, as this note originally (incorrectly)
  claimed.

- **BLE provisioning stub:** `components/ble_provision/` logs a warning. Implement
  using `$IDF_PATH/examples/provisioning/wifi_prov_mgr` as reference.

- **Zigbee stub:** `components/zigbee/` logs a warning. Valid only on ESP32-H2 and
  ESP32-C6. Requires `idf.py add-dependency "espressif/esp-zigbee-sdk"`.

- **OTA uses HTTP in dev, must switch to HTTPS in production:** `CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP=y`
  is set in `sdkconfig.defaults` to allow testing against a local HTTP server. A product
  fork must set it to `n` and add a CA cert bundle (`CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y`
  plus Espressif cert bundle or a custom cert) before shipping.
