# Reproduction Commands

These commands document provenance only. Do not rerun training unless intentionally regenerating artifacts.

**Scope note:** this package now exports a single model (Coarse Tree). Model A (Subspace KNN)
provenance is retained below for historical reference only; its artifacts are not present in
this firmware repo and it is not part of the current export scope. See `MODEL_SELECTION.md`.

## Coarse Tree Export Provenance (selected model)

```powershell
python -m weldmltrainer.parse_and_clean --input-dir I:\Documents\DevDocs\mm_Matlab\weld_1\Dat1_NP_FL --segment-start sposm_ge_zero --feature-groups time fft cwt
python -m weldmltrainer.run_loocv_models --dataset data_processed/WSU_TM_Ans_Dat_full_3.csv --save-top 999
```

Relevant artifacts:

- `results/model_summary_table_20260428_083434Z.csv`
- `results/saved_models/20260428_083434Z_coarse_tree.joblib`
- `results/saved_models/20260428_083434Z_coarse_tree.json`
- `results/coarse_tree_20260428_083434Z_loocv.csv` (per-sample LOOCV fold predictions)
- `src/results/run_history.csv`

## Verification Performed For This Export

The selected Coarse Tree artifact (`results/saved_models/20260428_083434Z_coarse_tree.joblib`)
was loaded and scored against its own training set,
`data_processed/WSU_TM_Ans_Dat_full_3.csv` (38 samples) — the same distribution the golden
vectors are drawn from. It scored 100% (38/38), including `l041.fsj` and `l085.fsj`, which are
this model's two LOOCV held-out errors (94.7%, 36/38) per
`results/coarse_tree_20260428_083434Z_loocv.csv`. Both figures are real and are not in conflict
— see `MODEL_SELECTION.md` "Accuracy — two distinct numbers, do not conflate". No training was
rerun; no controlled-gap evaluation was performed as part of this export (out of scope for this
POC).

## Model A Export Provenance (excluded from current scope, retained for reference)

```powershell
python -m weldmltrainer.run_loocv_models --dataset data_processed/WSU_Tm_Dat_legacy_full.csv --save-top 999 --force-save-models
python -m weldmltrainer.evaluate_saved_models --dataset data_processed/WSU_Tm_Ans_Dat_gap.csv --prefix 20251219_033801Z
```

Relevant artifacts:

- `results/saved_models/20251219_033801Z_subspace_knn.joblib`
- `results/saved_models/20251219_033801Z_subspace_knn.json`
- `results/saved_model_evaluations/evaluation_summary_20260428_092239Z.csv`
