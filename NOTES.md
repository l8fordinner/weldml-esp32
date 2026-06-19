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
| Boot log capture via RFC2217 | PASS | Captured via passive read on /dev/ttyACM1 (DTR/RTS=False); full boot log below |

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

**RFC2217 monitoring note:** After RST, board re-enumerated as `/dev/ttyACM1` (not `/dev/ttyACM0`).
Portal auto-restarted RFC2217 server on `/dev/ttyACM1:4003`. Passive read works when DTR/RTS=False.
`plain_rfc2217_server.py` holds DTR=1/RTS=1 by default — triggers download mode if board is reset
while serial client is connected with those states. Workaround: set DTR=False/RTS=False before
reading, or use manual RST press. This is a workbench infrastructure issue, not a firmware bug.

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

- **STA connection failure fallback:** If station credentials are saved but connection
  fails after `WIFI_MAX_RETRY` attempts, the device logs an error but does not fall
  back to SoftAP. A product fork should decide the retry/fallback strategy.

- **SPIFFS web files not auto-built on first clone:** The SPIFFS image is generated by
  `idf.py build` and flashed by `idf.py flash`. Nothing extra is needed, but this is
  not obvious to new contributors.

- **BLE provisioning stub:** `components/ble_provision/` logs a warning. Implement
  using `$IDF_PATH/examples/provisioning/wifi_prov_mgr` as reference.

- **Zigbee stub:** `components/zigbee/` logs a warning. Valid only on ESP32-H2 and
  ESP32-C6. Requires `idf.py add-dependency "espressif/esp-zigbee-sdk"`.

- **OTA uses HTTP in dev, must switch to HTTPS in production:** `CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP=y`
  is set in `sdkconfig.defaults` to allow testing against a local HTTP server. A product
  fork must set it to `n` and add a CA cert bundle (`CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y`
  plus Espressif cert bundle or a custom cert) before shipping.
