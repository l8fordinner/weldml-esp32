# WeldML Milestone 2 Requirements — Cloud Connectivity + Local Web UI

Additive to the MVP (`MVP_REQUIREMENTS.md`), which stays closed and unchanged. Full decision
rationale for everything below is in `OPEN_QUESTIONS.md` Q11–Q15.

---

## Purpose

Support a live demo in the university's Industry 4.0 lab: a local web UI on the ESP32 shows
recent weld results, a button uploads them to the "Industry 4.0 Lab" tenant on ThingsBoard
(`iot.mwe-inc.com`), and a button clears old `.fsj` files from the SD card.

---

## Data Flow

```
weld_processor (existing MVP pipeline, unchanged)
    │
    ▼
weldml_results.csv (existing, unchanged) ──► cached recent rows (RAM/NVS)
    │                                              │
    ▼                                              ▼
.fsj files accumulate on SD              ESP32 webserver (new)
    │                                       ├─ GET /  → results table (from cache)
    ▼                                       ├─ POST /api/upload → HTTPS to ThingsBoard
delete-requested flag,                     └─ POST /api/clear   → sets delete-requested flag
honored only during the
next Q3 write-idle window
```

The webserver never opens the SD card directly (Q12). All SD interaction — caching recent
results for display, and honoring a delete request — happens inside the existing
write-idle-triggered processing window `weld_processor` already owns per Q3/Q6.

---

## Local Web UI

- Served from the ESP32 over WiFi station mode, using the existing (currently unwired)
  `components/webserver/` scaffolding — SPIFFS for static assets, plus new product-specific
  REST endpoints (this project's own code, not modifications to the generic component).
- WiFi provisioning: adapt the SoftAP-fallback + `/api/wifi` pattern already implemented in the
  sibling `esp32-base-template` repo's `main.c` and `components/webserver/webserver.c`
  (currently unused in this repo).
- Requires adding a SPIFFS partition to
  `boards/waveshare-esp32-s3-lcd-147/partitions.csv` (currently single-factory-app, no OTA/
  SPIFFS — 16 MB flash with ~13 MB free, ample room).
- **Display:** full history table from the cached `weldml_results.csv` rows (Q6) — not just
  the latest result.
- **Upload button:** POSTs cached rows not yet uploaded (NVS watermark = last-uploaded row
  index) to ThingsBoard over HTTPS. Shows inline status: "Uploading…" / "Uploaded N records" /
  "Failed: <reason>". On failure, the watermark does not advance (Q9, Q13).
- **Clear button:** sets a delete-requested flag; deletion itself happens on the next
  `weld_processor` write-idle cycle (Q12), removing `.fsj` files older than the most recent
  N=20 (tunable), never touching `weldml_results.csv` (Q14).

---

## ThingsBoard Upload

- Protocol: HTTPS only (ThingsBoard HTTP Device API), not MQTT — campus firewalls are likely to
  block MQTT's non-standard TLS port, and the upload pattern (manual batch, not streaming)
  doesn't benefit from MQTT's persistent connection anyway (Q13).
- `POST https://iot.mwe-inc.com/api/v1/$ACCESS_TOKEN/telemetry`, ESP-IDF `esp_http_client` +
  `esp_crt_bundle` for TLS certificate validation.
- Payload: structured per-weld results only — the 22-feature vector, PASS/FAIL, and existing
  `weldml_results.csv` metadata columns. Raw `.fsj` waveform data is **not** uploaded (Q13).
- Payload also includes `pass_flag`/`fail_flag` (1/0, mutually exclusive) alongside
  `predicted_class` — required by the ThingsBoard dashboard's PASS/FAIL running-total widget.
  ThingsBoard's `Sum` aggregation reduces a time window to a single already-summed value before
  any data post-processing function runs, so a post-processing function on the raw
  `predicted_class` key cannot derive a running PASS/FAIL split (verified live against
  `iot.mwe-inc.com` 2026-08-05/06 — see `THINGSBOARD_SETUP.md` Section 5). Deriving separate
  pre-split flag keys server-side would need Rule Engine scripting; uploading them directly from
  firmware avoids that with negligible payload cost.
- Device access token stored in NVS, configured the same way as the existing (unused)
  `mqtt_url`/`ota_url` fields in `components/webserver/webserver.c`'s `/api/config` endpoint —
  extend that endpoint with a `tb_token` field rather than inventing a new config mechanism.
- `components/app_mqtt/` is explicitly **not** used for this milestone (Q13) — left scaffolded
  for a possible future streaming use case.

---

## ThingsBoard Side (admin, not firmware)

See `docs/THINGSBOARD_SETUP.md` for the step-by-step instructions. Summary:

- Tenant "Industry 4.0 Lab" already exists (ThingsBoard CE v4.3.1.3 at `iot.mwe-inc.com`).
- Dedicated tenant profile (not `default`) with containment limits against a security breach or
  runaway device — see Q15 and `THINGSBOARD_SETUP.md` for exact values.
- Device entity created under the tenant for the ESP32; access token retrieved for firmware
  NVS config.
- Dashboard created/imported and branded at the dashboard level (logo, color theme, custom
  widgets) — **not** platform-wide White Labeling, which is a paid ThingsBoard PE-only feature
  and isn't needed for what was asked (Q10).

---

## Out of Scope for Milestone 2

- OTA firmware updates (still separately out of scope — not requested)
- MQTT transport (HTTPS only, see above)
- Raw `.fsj` waveform upload to ThingsBoard (structured results only)
- Continuous/automatic upload (manual button-triggered batch only)
- Platform-wide ThingsBoard White Labeling (dashboard-level branding only)
- BLE provisioning
- On-demand SD ownership transitions triggered by web UI actions (Q12 Option C, rejected)
- Truncating/rotating `weldml_results.csv` from the clear button (Q14)

---

## Board Target

Unchanged: Waveshare ESP32-S3-LCD-1.47, `CONFIG_BOARD_HAS_WIFI=y` already set in
`boards/waveshare-esp32-s3-lcd-147/sdkconfig.defaults`.
