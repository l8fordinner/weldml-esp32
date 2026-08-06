# WeldML Milestone 3 Requirements — OTA Firmware Updates

Additive to Milestone 2 (`MILESTONE_2_REQUIREMENTS.md`), which stays closed and unchanged, and
additive to the MVP (`MVP_REQUIREMENTS.md`), which also stays closed. Full decision rationale for
everything below is in `OPEN_QUESTIONS.md` Q16–Q21.

**Depends on Milestone 2 being implemented first** — both milestones share the same WiFi
bring-up, partition table, and `components/webserver/` infrastructure, and redoing that
infrastructure twice would be wasted work. Milestone 3 is a layer on top of Milestone 2, not a
parallel track.

---

## Purpose

Let the ESP32 pull firmware updates over the air from ThingsBoard's built-in OTA package
distribution feature, triggered manually from the local web UI, without risking the device
getting bricked by a bad update and without slowing down the weld-processing pipeline (SD
parse → feature extraction → FFT → inference) that this project's whole MVP exists to run
correctly and quickly (~6 s cycle time, per `NOTES.md`'s performance investigation).

---

## Scope Decision: BLE Dropped

BLE was originally raised alongside OTA for this milestone but is **not included**. Milestone 2
already fully specs WiFi setup via a SoftAP-fallback pattern (Q11/Q15-era decisions) — that
already solves device provisioning. Adding BLE provisioning on top would solve a problem that
doesn't exist. The scaffolded-but-unimplemented `components/ble_provision/` stub stays as-is
(unused, matching its current state) unless a concrete future need for it appears — see Q16.

---

## OTA Update Flow

- **Delivery backend: ThingsBoard OTA packages**, not a self-hosted firmware server. The device
  is already talking to `iot.mwe-inc.com` over HTTPS for telemetry (Milestone 2, Q13) — reusing
  that connection for firmware avoids standing up separate infrastructure.
- **Protocol: plain HTTPS, no MQTT** — consistent with Milestone 2's existing HTTP-only decision.
  Confirmed via ThingsBoard's current documentation that HTTP-transport devices are supported for
  OTA (not MQTT-only): the device reads the `fw_title`/`fw_version`/`fw_checksum`/
  `fw_checksum_algorithm` shared attributes via `GET /api/v1/$ACCESS_TOKEN/attributes`, then
  downloads the binary via `GET /api/v1/$ACCESS_TOKEN/firmware?title=...&version=...` — the
  `chunk`/`size` query params are optional, so a single plain GET returns the whole image,
  compatible with ESP-IDF's `esp_https_ota()` with moderate changes (dynamic URL construction
  instead of the existing scaffolding's static configured URL, plus checksum verification).
- **Trigger: manual, button-triggered only** — no background polling, no unattended
  self-update. Matches the existing generic `components/ota/` scaffolding's philosophy and
  Milestone 2's manual-upload-button precedent (Q13).
- **Update-availability check: runs once per `/ota` page load**, not on a background timer.
  Fetches the ThingsBoard shared attributes, compares against the running version (see
  Versioning below), and sets the web UI's Update button state:
  - Update available → button reads **"Update available: vX.Y.Z"**, enabled.
  - Current → button reads **"Up to date"**, greyed out / disabled.
  This replaces the existing generic `ota.html`'s blind "always trigger a pull on click"
  behavior — that template default is not used for this product.
- **Versioning: ESP-IDF git-describe** (`CONFIG_APP_PROJECT_VER_FROM_GIT`). No versioning scheme
  exists in this repo today (confirmed: no `PROJECT_VER`, no `VERSION` file, no git tags) — this
  needs adopting git tags for releases going forward. Chosen over a hand-maintained `VERSION`
  file or a build counter because it ties every running firmware build to an actual commit,
  matching this project's existing "verified on hardware" traceability convention (`NOTES.md`
  already references specific commits per hardware test).
- **Checksum verification: hard abort on mismatch.** If the downloaded firmware's checksum
  doesn't match ThingsBoard's `fw_checksum` attribute, the image is never marked bootable —
  full stop, not "flash and hope the rollback health-check catches it." Redundant with the
  rollback safety net below, deliberately: never even attempt to boot known-corrupt firmware.
- **Rollback safety: yes.** Uses ESP-IDF's app rollback mechanism
  (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`) — a newly-flashed image stays in "pending verify"
  state until the app explicitly calls `esp_ota_mark_app_valid_cancel_rollback()`; if it never
  does (crash, hang, failed init), the bootloader automatically reverts to the previous good
  partition on the next reset. "Booted healthy" is defined as **reaching the existing point in
  `main.c` where LCD init and SD/USB-MSC init both succeed and `weld_processor_start()` is
  called** — this is already this project's existing definition of "didn't fail" (today, LCD/SD
  init failure already halts and shows solid RED). Mark-valid is called right after that point,
  early in boot.

---

## New Component: `ota_policy`

A small pure-logic component (no ESP-IDF/FreeRTOS dependencies), following the same pattern as
`weld_parser`, `weld_inference`, and Milestone 2's `weld_cloud`. Not folded into `weld_cloud`,
since firmware-update policy is a distinct concern from weld-result upload/retention — matches
this project's one-component-per-concern convention. Owns:

- **Version comparison**: given the running version string and ThingsBoard's `fw_version`
  attribute, decide whether an update is available.
- **Checksum verification**: given downloaded firmware bytes (or a running hash) and
  ThingsBoard's `fw_checksum`/`fw_checksum_algorithm` attributes, decide pass/fail.

Host-tested the same way `weld_parser`/`weld_inference`/`weld_cloud` are — zero hardware
mocking, run via the existing host test runner. Everything else (the actual HTTPS attribute
fetch, the actual `esp_https_ota()` call, the actual rollback mark-valid call) stays thin,
hardware-verified-only glue, matching the existing `weld_processor` boundary.

---

## CPU-Contention Protection

**The problem (confirmed, not hypothetical):** `weld_mon` (the task that runs SD parsing,
esp-dsp FFT, and inference), the existing scaffolded `ota_task`, and ESP-IDF's HTTP server
(`webserver.c`'s `HTTPD_DEFAULT_CONFIG()`) all default to the same FreeRTOS priority (5), and
nothing in this codebase pins any task to a specific core. Left as-is, an active OTA download or
webserver request handling could round-robin-share CPU time with `weld_mon` at exactly the
moment it needs to run its ~6 s FFT/inference cycle, slowing down weld processing.

**Fix — two layers:**

1. **Primary: bracket the existing write-idle window.** `weld_processor` already has a state
   machine with an exact "SD is exclusively mine, nothing else should be competing for
   resources" window, from Q3/Q6/Q12: `WAITING` (idle/cyan) → `WRITING` (white) → `PROCESSING`
   (blue) → `SUCCESS`/`FAILURE` → back to `WAITING`. Extend that same window: call
   `webserver_stop()` the moment a write is detected (entering `WRITING`), and
   `webserver_start()` again once back to `WAITING`. This gives a *stronger* guarantee than
   priority tuning alone — zero HTTP/WiFi-driven task activity can exist during the FFT/inference
   window at all, not just "lower priority than weld_mon." WiFi association itself stays up the
   whole time (only the HTTP server stops/starts) — no WiFi re-association latency, which would
   otherwise cost 1–5+ seconds per weld cycle.
   - This also means OTA can't be triggered during this window "for free" — OTA is only
     reachable through the web UI, and the web UI is down during `WRITING`/`PROCESSING`.
   - Confirms and matches the product's actual usage pattern: the local web UI is for viewing
     recent weld results and triggering upload/OTA/cleanup *while idle* — between individual
     welds, or once a welding session on the robot is finished. It is explicitly not needed
     (and is deliberately turned off) during the moment a weld file is actively being written
     and processed.
2. **Defense in depth: priority + core pinning.** Raise `weld_mon`'s task priority above the
   WiFi/httpd/OTA default (5) — e.g. 10+ — and pin it to core 1 (ESP-IDF's WiFi driver defaults
   its own internal tasks to core 0, so keeping `weld_mon` on the other core is a stronger
   separation than priority alone). This covers the edge case where `webserver_stop()` is called
   but a request is already mid-flight.

**Verification requirement:** this is architecture reasoned from ESP-IDF's documented scheduler
behavior, not yet hardware-verified. Once Milestone 2/3 are implemented, the existing l060.fsj/
l046.fsj timing tests (the same ones that measured the current ~6 s cycle) must be re-run with
WiFi/webserver active, and `NOTES.md` updated with the result, before claiming the fix works. Do
not claim this works from code review alone.

---

## Flash Partition Table

16 MB flash, ~13 MB free beyond the existing `nvs`/`phy_init` partitions (current single
`factory` app partition, 3 MB, being replaced):

| Partition | Type | Size | Purpose |
|---|---|---|---|
| `nvs` | data/nvs | 24 KB (unchanged) | Existing config storage |
| `phy_init` | data/phy | 4 KB (unchanged) | Existing PHY calibration data |
| `otadata` | data/ota | 8 KB | Required by ESP-IDF's OTA boot-select mechanism |
| `ota_0` | app/ota_0 | 3 MB | First OTA app slot |
| `ota_1` | app/ota_1 | 3 MB | Second OTA app slot |
| `spiffs` | data/spiffs | 2 MB | Milestone 2's web UI static assets |

Total ≈ 8.4 MB of 16 MB, leaving a comfortable margin. `ota_0`/`ota_1` sized to match the
current single-factory-app partition's headroom (current binary is ~407 KB; even after adding
WiFi/HTTPS/webserver/OTA, ~7x growth room remains).

---

## Out of Scope for Milestone 3

- BLE provisioning (see Scope Decision above — SoftAP-fallback already solves this)
- Automatic/unattended OTA (manual button-trigger only)
- Background/periodic update-availability polling (checked once per `/ota` page load only)
- A self-hosted firmware server (ThingsBoard's OTA package feature is the delivery backend)
- Booting firmware that fails checksum verification, under any circumstance

---

## Board Target

Unchanged: Waveshare ESP32-S3-LCD-1.47. `CONFIG_BOARD_HAS_BLE=y` stays in
`sdkconfig.defaults` as an accurate hardware-capability fact (the chip does support BLE) even
though no BLE feature is implemented this milestone.
