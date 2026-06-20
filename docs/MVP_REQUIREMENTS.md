# WeldML MVP Requirements

## First Milestone: Local File Read → Inference → Screen Status

The MVP is entirely self-contained on the ESP32-S3. No network, no MQTT, no web server,
no OTA in this phase. The board reads a weld data file from SD, runs inference, and shows
the result as a full-screen color on the 1.47" LCD.

---

## Data Flow

```
SD card (weld file)
    │
    ▼
Read & parse
    │
    ▼
Compute 22 WeldML features
    │
    ▼
Run Model A + Model B (both, independently)
    │
    ▼
Decision rule → BAD or GOOD
    │
    ▼
LCD full-screen color (primary feedback)
LED color (secondary)
```

---

## Input

- One weld data file accessible to firmware on the SD card.
- File format and location TBD (see OPEN_QUESTIONS.md).
- File must be available after the host robot completes writing and signals completion.
- SD ownership transitions from USB MSC (host-mounted) to firmware (local mount) before
  firmware reads. Safe handoff strategy required.

---

## Feature Extraction

Compute exactly **22 WeldML features** from the parsed weld file.

Feature definitions and computation logic are in the WeldML domain and must be ported
or re-implemented in C for the ESP32-S3. Source reference: existing WeldML model pipeline.

---

## Inference Models

Two models run independently on every weld file:

| Model | Name | Training accuracy |
|-------|------|------------------|
| Model A | Subspace KNN | 100% (controlled-gap historical dataset) |
| Model B | Coarse Tree | 94.7368% LOOCV |

Both models must be embedded in firmware (coefficients, tree structure, or lookup tables
as appropriate). Neither requires network access.

---

## Decision Rule

**Dual-model rescue policy (confirmed 2026-06-20 — Stage 6 planning):**

```
PASS if Model B predicts NP  OR  Model A predicts NP
FAIL if Model B predicts IF  AND Model A predicts IF
```

Model B (Coarse Tree, LOOCV-validated) is the primary model.  
Model A (Subspace KNN, gap-dataset-validated) is the secondary/rescue model.

Evaluation order:
1. Run Model B. If Model B predicts NP → PASS immediately.
2. If Model B predicts IF → run Model A.
3. If Model A predicts NP → PASS (Model A rescues).
4. If Model A also predicts IF → FAIL (both agree).

Equivalent: `PASS = (B == NP) OR (A == NP)` / `FAIL = (B == IF) AND (A == IF)`

**Note:** An earlier draft of this document stated `GOOD if A==NP AND B==NP`, which is the opposite (more conservative, both-must-agree) policy. That policy is incorrect. The dual-model rescue policy above is the authoritative Stage 6 decision rule.

---

## Output — Primary: LCD Screen

The 1.47" ST7789V3 LCD (172×320, SPI) is the primary output, intended to be visible
from across the room.

| State | Screen |
|-------|--------|
| Processing / busy | White (full screen) |
| Result: BAD (IF) | Red (full screen) |
| Result: GOOD (NP) | Green (full screen) |

Full-screen color fills are sufficient. No text, icons, or progress bars are required
for the MVP. The color must be unambiguous at room distance.

LCD hardware interface:
- Controller: ST7789V3 (may also be GC9307N on some production runs)
- MOSI: GPIO45 · SCLK: GPIO40 · CS: GPIO42 · DC: GPIO41
- RST: GPIO39 · Backlight: GPIO48 (via N-MOSFET, PWM-capable)

---

## Output — Secondary: RGB LED

The WS2812B RGB LED on GPIO38 provides secondary status, visible up close.

| State | LED |
|-------|-----|
| Processing | White blink (matches SmrtUsbEsp convention) |
| BAD | Red |
| GOOD | Green |

The LED is a supplement to the screen, not a replacement. Screen color is the
authoritative signal for the MVP.

---

## Out of Scope for MVP

These features are **desired for later milestones** but explicitly excluded from the MVP:

- OTA firmware updates
- MQTT / backend database upload
- WiFi provisioning or station mode
- Web server (config, status, OTA UI)
- BLE provisioning
- Multi-file batch processing
- Weld result history / logging to SD
- USB Mass Storage passthrough (SmrtUsbEsp mode can remain in a separate branch)

---

## Board Target

Waveshare ESP32-S3-LCD-1.47 with verified hardware facts in PRIOR_WORK_CONTEXT.md
and the schematic (docs/ESP32-S3-LCD-1.47_schematic_diagram.pdf).

Build system decision pending (see OPEN_QUESTIONS.md): native ESP-IDF or PlatformIO.
