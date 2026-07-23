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
Run Coarse Tree model
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

## Inference Model

**Single-model proof-of-concept (rescoped 2026-07-14 — supersedes the dual-model plan below).**

One model runs on every weld file:

| Model | Name | Accuracy |
|-------|------|----------|
| Coarse Tree | `sklearn.tree.DecisionTreeClassifier(max_depth=2)`, run `20260428_083434Z`, `sposm_ge_zero` policy | 94.7% LOOCV (36/38, honest held-out estimate) / 100% resubstitution on the deployed (refit-on-all-38) artifact — these are two distinct figures, do not conflate; see `model_exports/esp32_port/MODEL_SELECTION.md`. |

Model must be embedded in firmware (tree node table — 5 nodes, max depth 2). No network access
required. No KNN, no ensemble voting, no per-estimator feature-subset storage — the previously
planned Model A (Subspace KNN, 200-estimator BaggingClassifier) is **out of scope** for this POC;
see rationale below. This significantly simplifies the embedded implementation versus the
dual-model plan: no RAM-bounded distance streaming, no training-vector storage in flash.

**Known, accepted limitation:** this model is validated against its own training distribution
(the 38-sample `original38` set) only. It is **not** validated against, and is not claimed to
generalize to, the controlled-gap distribution — no model evaluated in the trainer repo achieves
both LOOCV ≥ 80% and GAP ≥ 80% simultaneously (systematic inversion; the same 94.7%-LOOCV tree
family predicts NP for essentially every gap-set IF defect). This is a proof-of-concept
demonstrating a real, deployable classifier at its documented accuracy on its target distribution,
not a claim of generalization beyond it. Full rationale:
`WeldMLTrainer-PyTorch/docs/reports/MODEL_SELECTION_DECISION_CURRENT.md`.

---

## Decision Rule

**Single-model policy (rescoped 2026-07-14):**

```
PASS if Coarse Tree predicts NP
FAIL if Coarse Tree predicts IF
```

No second model, no rescue step, no AND/OR cascade.

---

## Superseded — Prior Dual-Model Plan (historical, do not implement)

The dual-model rescue policy below was the Stage 6 decision rule through 2026-07-13. It is
retained here only for history; do not implement it. It was replaced because the trainer-side
project confirmed no combination of the two models (OR-cascade, AND-cascade, either evaluation
order) achieves both good LOOCV and good GAP performance — the cascade was evaluated and
rejected on its own terms (`WeldMLTrainer-PyTorch/docs/reports/OLD_POLICY_CASCADE_EVAL.md`),
not just simplified away for embedded convenience.

Old policy (do not implement):
```
PASS if Model B predicts NP  OR  Model A predicts NP
FAIL if Model B predicts IF  AND Model A predicts IF
```
Model B = Coarse Tree (LOOCV-validated, primary). Model A = Subspace KNN (gap-dataset-validated,
secondary/rescue, only invoked if Model B predicted IF).

**Note:** An even earlier draft of this document stated `GOOD if A==NP AND B==NP` (the opposite,
more-conservative both-must-agree policy) — also not implemented, also superseded.

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
