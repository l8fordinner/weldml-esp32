# ESP32 Base Template — Project Requirements

This document captures the architecture decisions and requirements for this project.
It is intended to be read by Claude Code before scaffolding or building anything.

When used as a fork, also read PROJECT.md and any documents in the docs/ folder
before taking any action.

---

## Overview

This is a **public, open-source ESP32 base template** built on ESP-IDF and FreeRTOS.
It is designed to be forked — first into a private branded template, then into individual products.
The template should be clean, well-documented, and make no assumptions about the backend or brand.

The goal is eventually to release this publicly. Keep code clean, well-structured, and
well-documented with that in mind.

---

## Three-Tier Architecture

```
esp32-base-template/        ← this repo (public, open source)
  └── generic components
  └── basic example web UI
  └── no backend assumptions

brand-template/             ← private fork of base (not this repo)
  └── branded web UI
  └── backend-specific integration (e.g. ThingsBoard CE, self-hosted)
  └── brand defaults and config

product/                    ← private fork of brand-template (not this repo)
  └── product-specific features only
  └── docs/ populated with product datasheets, schematics, etc.
```

**Important boundary rules:**
- ThingsBoard or any other backend platform belongs in the brand fork, NOT here
- Branded UI belongs in the brand fork, NOT here
- Product-specific sensors, drivers, or features belong in the product fork, NOT here
- This template makes zero assumptions about backend, brand, or product

---

## License

**Apache 2.0** — consistent with ESP-IDF and other dependencies in the stack.

---

## Build System

**Native ESP-IDF** (not PlatformIO).

Reasons:
- Full Kconfig support — essential for per-chip capability flags
- `idf_component.yml` component system works without workarounds
- Zigbee, BLE 5, and ESP-IDF-specific components are first-class citizens
- Multiple chip targets are natively supported
- PlatformIO wraps ESP-IDF anyway and lags behind on newer chip support

The ESP-IDF VS Code extension is the recommended editor integration.
Claude Code handles build/flash/monitor via `idf.py` CLI.

---

## Target Hardware

The template must support **multiple ESP32 chip variants**. Chip support is structured
so adding a new chip variant is straightforward.

Initial chip targets:

| Chip | Notes |
|------|-------|
| ESP32 | Dual core, classic BT + BLE, no native USB |
| ESP32-S3 | Dual core, native USB, BLE 5, AI acceleration |
| ESP32-C3 | Single core RISC-V, native USB, BLE 5, low cost |
| ESP32-H2 | No WiFi, Zigbee/Thread native, BLE 5 |
| ESP32-C6 | WiFi 6, Zigbee, Thread, BLE 5 — most capable |

Board support lives in a `boards/` directory with per-board `sdkconfig.defaults`
and `board.h` files that define pin assignments and hardware capabilities via Kconfig flags:

```
CONFIG_BOARD_HAS_ZIGBEE
CONFIG_BOARD_HAS_NATIVE_USB
CONFIG_BOARD_DUAL_CORE
CONFIG_BOARD_HAS_BT_CLASSIC
```

Optional components gate themselves behind these flags so invalid configurations
are caught at build time rather than at runtime.

---

## Always-Included Components

Every product built from this template includes these:

| Component | Notes |
|-----------|-------|
| FreeRTOS | Foundation — ESP-IDF runs on it anyway |
| ESP-IDF | Hardware abstraction, component system |
| WiFi | Practically every product needs this |
| NVS | Config persistence (non-volatile storage) |
| Webserver | Serves the setup/config UI |
| Web UI | Basic generic example (see below) |
| MQTT client | Generic — broker URL is user-configured via NVS/web UI |
| OTA | Firmware update via HTTPS pull |

---

## Optional Components

Products opt into these via `idf_component.yml` and Kconfig:

| Component | Notes |
|-----------|-------|
| BLE provisioning | For products that provision over Bluetooth |
| Zigbee | Only valid on H2 or C6 natively — Kconfig guards this |

---

## Web UI

The base template includes a **basic, generic web UI** served from the ESP32 webserver.
Intentionally plain — no logo, placeholder colors — but fully functional.

Served as static files from flash (LittleFS or SPIFFS). Plain HTML/CSS/JS is fine —
no React or Vue required for the base. The branded fork is where a polished UI lives.

The example UI must demonstrate the pattern for:
- WiFi provisioning (enter SSID/password)
- Basic device config page (MQTT broker URL, port, credentials)
- OTA trigger (manual update button)
- Device status/info page

---

## MQTT

Generic MQTT client — no backend assumptions:
- Broker URL, port, and credentials configurable via NVS and web UI
- No hardcoded topic structure — that belongs in the brand fork
- Must handle reconnection gracefully
- ThingsBoard topic conventions, provisioning flows, etc. belong in the brand fork

---

## OTA

Simple, backend-agnostic pull-based OTA:
- Device fetches firmware from a configurable HTTPS URL
- Trigger via: MQTT message OR manual button in web UI
- Status reporting: idle / downloading / success / failed
- Uses ESP-IDF's `esp_https_ota` component

**Critical:** Partition table must be configured for OTA from day one (two app partitions).
This is painful to change after devices are deployed — get it right in the template.

More sophisticated OTA flows (update notifications, subscription gating, automatic push
from server, phone client) are platform/product features. They belong in the brand or
product layer, not here.

---

## Repo Structure

```
esp32-base-template/
  components/
    app_mqtt/                 ← MQTT client wrapper (named app_mqtt to avoid
      CMakeLists.txt            shadowing IDF's built-in mqtt component)
      mqtt.c
      mqtt.h
    webserver/
      CMakeLists.txt
      webserver.c
      webserver.h
    ota/
      CMakeLists.txt
      ota.c
      ota.h
    ble_provision/            ← optional component (stub)
      CMakeLists.txt
      ble_provision.c
      ble_provision.h
    zigbee/                   ← optional component (stub, H2/C6 only)
      CMakeLists.txt
      zigbee.c
      zigbee.h
  boards/
    esp32/
      sdkconfig.defaults
      board.h
    esp32-s3/
      sdkconfig.defaults
      board.h
    esp32-c3/
      sdkconfig.defaults
      board.h
    esp32-h2/
      sdkconfig.defaults
      board.h
    esp32-c6/
      sdkconfig.defaults
      board.h
    examples/
      feather-huzzah32/       ← Adafruit Feather HUZZAH32 example board config
        sdkconfig.defaults
        board.h
  web/
    index.html                ← WiFi provisioning / setup UI
    config.html               ← device config (MQTT, etc.)
    ota.html                  ← OTA trigger and status
    status.html               ← device info and status
    style.css
    app.js
  docs/
    README.md                 ← instructs fork users to add datasheets,
                                 schematics, and hardware docs here.
                                 Claude Code will read this folder
                                 automatically when present.
  main/
    CMakeLists.txt
    main.c
    Kconfig.projbuild
  CMakeLists.txt
  sdkconfig.defaults          ← base defaults, boards override
  partitions.csv              ← OTA-ready partition table (two app partitions)
  idf_component.yml
  PROJECT.md.template         ← copy to PROJECT.md and fill in when forking
  README.md
  REQUIREMENTS.md             ← this file
  NOTES.md                    ← testing status and known gaps
  LICENSE
  .gitignore
```

### Implementation Notes

**MQTT component naming:** The local MQTT wrapper is in `components/app_mqtt/` rather
than `components/mqtt/`. ESP-IDF ships a built-in `mqtt` component; a local component
with the same name shadows it and breaks the build. `app_mqtt` wraps the IDF component.

**SPIFFS chosen over LittleFS:** The web UI is served from a SPIFFS partition.
LittleFS is an option for future consideration if wear levelling becomes important.

**SoftAP fallback added:** When no WiFi credentials are stored in NVS, `main.c`
starts a SoftAP (`ESP32-Setup`, open network) instead of attempting station mode.
This is the standard first-run provisioning pattern and was added during initial
scaffolding. SSID and AP parameters are configurable via Kconfig.

---

## Forking This Template

When creating a new project from this template:

1. Fork or copy this repo
2. Copy `PROJECT.md.template` to `PROJECT.md` and fill it in completely
3. Add datasheets, schematics, and any hardware reference docs to `docs/`
4. Add your board to `boards/` if it is not already there
5. Start Claude Code and run:
   > "Read REQUIREMENTS.md, PROJECT.md, and all documents in docs/ before doing anything.
   >  Then scaffold or extend the project as described in PROJECT.md."

Claude Code will read the hardware docs automatically and use them to inform
pin assignments, driver selection, and component configuration.

---

## Development Tooling

**Claude Code** is the primary development agent — it handles build, flash, and monitor
via `idf.py` CLI. Serial monitoring is run in a separate terminal.

**Optional: Universal ESP32 Workbench** (Andreas Spiess / SensorsIot on GitHub)
- A Pi Zero W2 acting as a networked test instrument for ESP32 boards
- Provides remote serial (RFC2217), WiFi test AP, GPIO control, UDP log capture,
  local OTA server, and BLE proxy — all via HTTP API
- Comes with Claude Code skills so Claude Code can operate it directly
- Not required for building the template, but valuable when testing provisioning
  flows, OTA, and multi-board scenarios
- Set up when testing begins, not before

---

## Project Management

**GSD:** Skip for the initial template build — REQUIREMENTS.md provides sufficient
structure. Start using GSD when beginning the first product fork, where the workspace
and seeds features become genuinely useful for managing improvements that need to
propagate back from products to the template.

---

## What Claude Code Should Do First (Template Build)

1. Read this document fully before taking any action
2. Scaffold the repo structure above
3. Create valid `CMakeLists.txt` files throughout (ESP-IDF component format)
4. Create an OTA-capable `partitions.csv` (two app partitions)
5. Create stub `board.h` and `sdkconfig.defaults` for each chip target
6. Create a minimal working `main.c` that initialises WiFi and the webserver
7. Create a basic but functional web UI covering the four pages listed above
8. Create `docs/README.md` with instructions for fork users
9. Create `PROJECT.md.template` for fork users to fill in
10. Verify the project builds cleanly with `idf.py build` for at least one target
11. Do not add product-specific code, backend integrations, or branding

---

## What Belongs in the Brand Fork (Not Here)

- ThingsBoard or any other backend platform integration
- Branded web UI (colors, logos, fonts)
- Specific MQTT broker URLs or topic conventions
- Business logic around subscriptions, user accounts, notifications
- Phone or mobile client

## What Belongs in the Product Fork (Not Here)

- Specific sensor or peripheral drivers
- Product-specific web UI pages beyond the base pattern
- Hardware pin assignments beyond what board.h covers
- Any feature unique to one product
- Product datasheets, schematics, and hardware reference docs (goes in docs/)
