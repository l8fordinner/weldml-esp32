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
