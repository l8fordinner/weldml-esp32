# ESP32 Porting Notes

This package is an export contract, not firmware. **Single-model proof-of-concept scope** — see
`MODEL_SELECTION.md` for the full rationale.

## Shared Runtime Contract

- Inputs must be the 22 raw features in `FEATURE_SCHEMA.json` order.
- Use class mapping `0 = NP`, `1 = IF`.
- No scaler or imputer is part of the selected artifact.
- Reject or handle NaN/Inf values before inference; training code did not define imputation behavior.

## Coarse Tree

Coarse Tree is the selected model. The exported tree has 5 nodes and max depth 2. It can be
implemented as nested comparisons or a static node table. Thresholds are raw feature thresholds
in the shared feature order.

## Items To Validate In The ESP32 Repo Later

- Floating-point parity between Python and ESP32 for all golden vectors in
  `golden_vectors/golden_vectors.json` — the firmware must reproduce the `model_predicted_class`
  / `model_probability_class1` fields (the real deployed-artifact output) exactly. Do not target
  the `loocv_held_out_prediction` fields; those are a separate held-out estimate, not this
  artifact's actual behavior.
- Feature extraction parity from raw weld logs or sensor stream to the 22-feature schema.
- This model is not validated against the controlled-gap distribution and must not be marketed
  or deployed as generalizing beyond the original38 training distribution; see
  `MODEL_SELECTION.md` "Why not both LOOCV and GAP performance?".

## Excluded From This Package

Model A (Subspace KNN) and any dual-model combination policy are out of scope for this
proof-of-concept. Artifacts have been relocated to `docs/reports/model_a_gap100_subspace_knn/`
(not present in this package); do not port or wire them into firmware. See
`MODEL_SELECTION.md` "Excluded from this package's scope" for why.
