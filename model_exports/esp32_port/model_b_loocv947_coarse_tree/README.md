# Coarse Tree — LOOCV 94.7% (Selected Model, Single-Model POC)

Source artifacts are copied into this directory. `portable_model.json` contains the C++-friendly tree node table, thresholds, leaf probabilities/classes, metrics, provenance, and ESP32 implementation notes.

This is the sole selected model for the current single-model ESP32 proof-of-concept — the April
2026 38-sample LOOCV robustness model with the simplest embedded port path
(`sposm_ge_zero` policy, run `20260428_083434Z`). See `../MODEL_SELECTION.md` for full rationale,
including the distinction between its 94.7% LOOCV estimate and its 100% resubstitution accuracy
on the deployed (refit-on-all-38) artifact, and for why no controlled-gap evaluation is included
in this scope.
