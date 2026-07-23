# ESP32 Port Model Selection

**Status: single-model proof-of-concept.** This package exports one model, scoped to the
original38 training distribution only. No dual-model policy, no GAP-set evaluation, and no
cascade logic is part of this export. See "Why not both" below for why.

## Selected Model - Coarse Tree (94.7% LOOCV)

- Model: Coarse Tree (`sklearn.tree.DecisionTreeClassifier(max_depth=2, random_state=42)`)
- Run prefix: `20260428_083434Z`
- Segment-start policy: `sposm_ge_zero` (current parser default)
- Trained on: `data_processed/WSU_TM_Ans_Dat_full_3.csv` (38 samples; refit on all 38 after LOOCV)
- Selected artifact: `results/saved_models/20260428_083434Z_coarse_tree.joblib`
- Metadata: `results/saved_models/20260428_083434Z_coarse_tree.json`
- Selection reason: tied for the best LOOCV accuracy under this run/policy (94.7%; six other
  models tied at the same run: Fine/Medium Tree, Bagged/Boosted/RUSBoosted Trees, Trilayered
  Neural Network), and the simplest embedded port path among the tied candidates - a single
  shallow decision tree, no ensemble voting, no matrix operations.

## Accuracy - two distinct numbers, do not conflate

- **LOOCV estimate: 94.7% (36/38).** Honest generalization estimate - each prediction made by a
  tree trained on the *other* 37 samples, never seeing the held-out sample. Per-sample fold
  predictions: `results/coarse_tree_20260428_083434Z_loocv.csv`. Two held-out errors:
  `l041.fsj` (false negative), `l085.fsj` (false positive).
- **Deployed artifact resubstitution accuracy: 100% (38/38)** on
  `data_processed/WSU_TM_Ans_Dat_full_3.csv`. This is the actual exported artifact - it was
  refit on all 38 samples (including l041 and l085), so scoring it against those same 38 rows
  is a resubstitution/training-accuracy check, not a generalization test. This is the number
  the ESP32 firmware must reproduce exactly; see `golden_vectors/golden_vectors.json` for the
  full per-sample explanation and both fields side by side.

## Why not both LOOCV and GAP performance?

No model evaluated in this project achieves both LOOCV >= 80% and GAP >= 80%
(`docs/reports/MODEL_SELECTION_DECISION_CURRENT.md`). High-LOOCV tree models, including this
one, invert systematically on the 8-sample controlled-gap holdout set - predicting NP for IF
defects and IF for NP blanks
(`docs/reports/TRAINING_RUN_FEATURE_EXTRACTION_INVENTORY.md`,
`docs/reports/94PCT_LOOCV_VS_SUBSPACEKNN_FULL_EVAL.md`). Pooling both distributions into a
combined 46-sample training set was also tested and did not resolve the inversion
(`docs/reports/COMBINED_SOURCE_LOOCV_RESULTS.md`).

This POC is deliberately scoped to the original38 training distribution only. It demonstrates a
real, deployable model performing at its documented LOOCV accuracy on the distribution it was
built for. It does not claim to generalize to the controlled-gap distribution - that limitation
is a finding to report, not a defect to be papered over with a cascade or a second model.

## Excluded from this package's scope

Not deployment concerns for this POC; documented in `docs/reports/` rather than carried inside
this export package:

- **Model A (Subspace KNN)** - 100% GAP / 55.3% LOOCV, runs `20251219_033801Z` /
  `20251219_033334Z`. Trained on a different, incompatible segment-start policy (LOADCELL
  legacy/pre-mode) and optimized for the GAP distribution, not the LOOCV distribution this POC
  targets. Artifacts relocated to `docs/reports/model_a_gap100_subspace_knn/`.
- **The GAP/`sposm_ge_zero` evaluation preview** - relocated to
  `docs/reports/GAP_SPOSM_GE_ZERO_EVALUATION_PREVIEW.md`.
- **Any OR/AND dual-model cascade policy** - evaluated and rejected; see
  `docs/reports/OLD_POLICY_CASCADE_EVAL.md` and `MODEL_SELECTION_DECISION_CURRENT.md` Section 4.
- **Combined-source (pooled 46-sample) training** - evaluated and did not resolve the LOOCV/GAP
  inversion; see `docs/reports/COMBINED_SOURCE_LOOCV_RESULTS.md`.
