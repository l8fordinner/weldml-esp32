# Reproduction Commands

These commands document provenance only. Do not rerun training unless intentionally regenerating artifacts.

## Model A Export Provenance

```powershell
python -m weldmltrainer.run_loocv_models --dataset data_processed/WSU_Tm_Dat_legacy_full.csv --save-top 999 --force-save-models
python -m weldmltrainer.evaluate_saved_models --dataset data_processed/WSU_Tm_Ans_Dat_gap.csv --prefix 20251219_033801Z
```

Relevant artifacts:

- `results/saved_models/20251219_033801Z_subspace_knn.joblib`
- `results/saved_models/20251219_033801Z_subspace_knn.json`
- `results/saved_model_evaluations/evaluation_summary_20260428_092239Z.csv`

## Model B Export Provenance

```powershell
python -m weldmltrainer.parse_and_clean --input-dir I:\Documents\DevDocs\mm_Matlab\weld_1\Dat1_NP_FL --segment-start sposm_ge_zero --feature-groups time fft cwt
python -m weldmltrainer.run_loocv_models --dataset data_processed/WSU_TM_Ans_Dat_full_3.csv --save-top 999
```

Relevant artifacts:

- `results/model_summary_table_20260428_083434Z.csv`
- `results/saved_models/20260428_083434Z_coarse_tree.joblib`
- `results/saved_models/20260428_083434Z_coarse_tree.json`
- `src/results/run_history.csv`

## Verification Performed For This Export

Both selected models were loaded from existing artifacts and scored against `data_processed/WSU_Tm_Ans_Dat_gap.csv`. Model A reproduced accuracy 1.0 with TP=6, TN=2, FP=0, FN=0. Model B was exported from the existing April 2026 LOOCV run; no training was rerun.
