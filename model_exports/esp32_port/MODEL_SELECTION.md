# ESP32 Port Model Selection

This package exports two existing WeldML models for later ESP32 planning. No final dual-model decision algorithm is designed in this repository.

## Model A - Historical Controlled-Gap Winner

- Model: Subspace KNN
- Selected artifact: `results/saved_models/20251219_033801Z_subspace_knn.joblib`
- Metadata: `results/saved_models/20251219_033801Z_subspace_knn.json`
- Alternate artifact also verified: `results/saved_models/20251219_033334Z_subspace_knn.joblib`
- Selection reason: reproduces 100% accuracy on the 8 controlled-gap samples.
- Verified controlled-gap metrics: accuracy 1.0, TP=6, TN=2, FP=0, FN=0.
- Caveat: this model is not selected for 38-sample LOOCV robustness.
- Version warning: artifact was pickled with sklearn 1.8.0; verification used sklearn 1.5.2; predictions still matched exactly.

## Model B - Current 38-Sample LOOCV Model

- Model: Coarse Tree
- Selected artifact: `results/saved_models/20260428_083434Z_coarse_tree.joblib`
- Metadata: `results/saved_models/20260428_083434Z_coarse_tree.json`
- Selection reason: achieved 0.9473684210526315 LOOCV accuracy and has the simplest embedded port path.
- LOOCV metrics from `results/model_summary_table_20260428_083434Z.csv`: TP=18, TN=18, FP=1, FN=1.

## Different Purposes

Model A and Model B serve different purposes. Model A preserves the historical controlled-gap behavior on the 8-sample gap set. Model B preserves the current April 2026 38-sample LOOCV robustness result in a small tree form suitable for embedded implementation.

Both models will be used later in new ESP32 repo planning. This export package provides the artifacts and contracts needed for that planning, without creating ESP32 application code here.
