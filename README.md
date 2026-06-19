# esp32-base-template

A public, open-source ESP32 base template built on ESP-IDF and FreeRTOS.
Designed to be forked into a private branded template, then into individual products.

See [REQUIREMENTS.md](REQUIREMENTS.md) for full architecture decisions and build system rationale.

---

## Current State

**Fully verified on Adafruit Feather HUZZAH32 (ESP32-WROOM-32, chip rev v3.0).**

| Area | Status |
|------|--------|
| Project structure | Complete |
| CMakeLists.txt (all components) | Complete |
| OTA partition table (dual app + SPIFFS) | Complete |
| Board configs (ESP32, S3, C3, H2, C6) | Complete |
| Example board: Feather HUZZAH32 | Complete |
| `main.c` — NVS, WiFi STA/AP, webserver, MQTT, OTA init | Complete |
| SoftAP fallback (first-run provisioning) | **Verified on hardware** |
| Web UI — 4 pages served from SPIFFS | **Verified on hardware** |
| Factory reset via web API | **Verified on hardware** |
| NVS persistence across reboots | **Verified on hardware** |
| MQTT client — connects to broker | **Verified on hardware** |
| OTA — HTTP pull, partition switch | **Verified on hardware** |
| BLE provisioning | Stub — implement when needed |
| Zigbee | Stub — implement when needed (H2/C6 only) |

See [NOTES.md](NOTES.md) for full test log and known gaps.

---

## First-Run Flow

On first boot with no WiFi credentials configured:

1. Device starts a SoftAP named **`ESP32-Setup`** (open, no password)
2. Connect your phone or laptop to **ESP32-Setup**
3. Browse to **`http://192.168.4.1`**
4. Enter your WiFi SSID and password, tap Save
5. Device saves credentials to NVS and reboots into station mode
6. Web UI is then available at the device's DHCP-assigned IP

---

## Building

Requires ESP-IDF v5.x. See [WSL_SETUP.md](WSL_SETUP.md) for Windows/WSL setup.

```bash
# Source ESP-IDF (once per terminal session)
. ~/esp/esp-idf/export.sh

# Build (app + SPIFFS web image)
idf.py build

# Flash everything to the board
idf.py -p /dev/ttyUSB0 flash

# Build and flash in one step
idf.py -p /dev/ttyUSB0 build flash
```

To target a specific board config:
```bash
BOARD=esp32 idf.py build
```

---

## Project Structure

```
esp32-base-template/
  components/
    app_mqtt/       ← MQTT client wrapper (wraps IDF's esp-mqtt)
    webserver/      ← HTTP server + SPIFFS, REST API
    ota/            ← Pull-based HTTPS OTA
    ble_provision/  ← Optional: BLE WiFi provisioning (stub)
    zigbee/         ← Optional: Zigbee (stub, H2/C6 only)
  boards/
    esp32/          ← Generic ESP32 sdkconfig.defaults + board.h
    esp32-s3/
    esp32-c3/
    esp32-h2/
    esp32-c6/
    examples/
      feather-huzzah32/   ← Adafruit Feather HUZZAH32 pinout + config
  web/              ← Static UI (served from SPIFFS at 192.168.4.1)
    index.html      ← WiFi setup
    config.html     ← MQTT broker config
    ota.html        ← OTA trigger + status
    status.html     ← Device info
    style.css / app.js
  main/
    main.c          ← app_main: NVS, WiFi STA/SoftAP, webserver, MQTT, OTA
    Kconfig.projbuild
    CMakeLists.txt
  docs/             ← Add datasheets and schematics here when forking
  CMakeLists.txt
  sdkconfig.defaults
  partitions.csv    ← OTA-capable: nvs / otadata / ota_0 / ota_1 / spiffs
  idf_component.yml
  PROJECT.md.template
```

---

## Creating a New Project from This Template

This repo is a GitHub Template Repository. To start a new project:

1. Click **"Use this template"** → **"Create a new repository"** on GitHub
2. Name your new repo, set visibility, and create it
3. Clone it into your Windows repos folder (see [WSL_SETUP.md](WSL_SETUP.md)):
   ```bash
   git clone https://github.com/your-org/your-project.git /mnt/j/ReposWSL/your-project
   ```
4. Open it in VS Code from WSL:
   ```bash
   cd /mnt/j/ReposWSL/your-project
   code .
   ```
5. Copy `PROJECT.md.template` to `PROJECT.md` and fill it in
6. Add datasheets, schematics, and hardware reference docs to `docs/`
7. Add your board to `boards/` if it is not already there (see "Targeting a Different Board" below)

Then start Claude Code and say:
> "Read REQUIREMENTS.md, PROJECT.md, and all documents in docs/ before doing anything.
>  Then scaffold or extend the project as described in PROJECT.md."

---

## Why Repos Live on the Windows Drive

This repo lives at `J:\ReposWSL\` on the Windows filesystem (mounted as `/mnt/j/ReposWSL/` in WSL) rather than in the WSL home directory (`~/`). Reasons:

- **Windows Explorer access** — you can browse, copy, and open files normally in Explorer
- **VS Code Remote WSL** — `code .` from WSL opens the folder with full WSL toolchain support, but the files remain accessible from Windows
- **No syncing needed** — one copy of the files, visible from both Windows and WSL
- **Backups** — standard Windows backup tools (OneDrive, robocopy, etc.) can see and back up the repos

WSL home (`~/`) is only reachable from Windows via `\\wsl.localhost\Ubuntu\home\username\` which is slower and less convenient.

---

## Targeting a Different Board

Board-specific configs live in `boards/<chip>/`. To target a different chip:

**1. Set the IDF target:**
```bash
idf.py set-target esp32s3   # or esp32c3, esp32h2, esp32c6
```

**2. Point to the right board defaults** in `sdkconfig.defaults` or override via `SDKCONFIG_DEFAULTS`:
```bash
SDKCONFIG_DEFAULTS="sdkconfig.defaults;boards/esp32-s3/sdkconfig.defaults" idf.py build
```

**3. Update `partitions.csv`** if the new board has more than 4MB flash — use `partitions.csv` (8MB+) rather than the default 4MB layout.

**4. Add a `board.h`** in `boards/examples/your-board/` with your actual GPIO pin definitions. Copy the Feather HUZZAH32 example as a starting point.

**5. Adjust `CONFIG_ESPTOOLPY_FLASHSIZE`** in the board's `sdkconfig.defaults` to match the actual flash size.

---

## Forking This Template (Brand / Product Fork)

1. Create a new repo from this template (see above)
2. Copy `PROJECT.md.template` to `PROJECT.md` and fill it in completely
3. Add datasheets, schematics, and hardware reference docs to `docs/`
4. Add or select the board config for your hardware
5. Before shipping: set `CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP=n` and add a CA cert bundle for HTTPS OTA

---

## License

Apache 2.0
