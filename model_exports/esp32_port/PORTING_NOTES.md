# ESP32 Porting Notes

This package is an export contract, not firmware.

## Shared Runtime Contract

- Inputs must be the 22 raw features in `FEATURE_SCHEMA.json` order.
- Use class mapping `0 = NP`, `1 = IF`.
- No scaler or imputer is part of either selected artifact.
- Reject or handle NaN/Inf values before inference; training code did not define imputation behavior.

## Model A: Subspace KNN

Model A is large for a microcontroller. It contains 200 KNN estimators, each using 14 selected features and 38 stored training vectors. A literal implementation should store feature subset indices, training labels, and training vectors in flash/PROGMEM and stream Euclidean distances to keep RAM bounded.

Voting/probability behavior should match sklearn BaggingClassifier: average per-estimator class probabilities, then choose the class with the highest averaged probability. The exported `portable_model.json` contains all feature subsets, vectors, labels, and parameters needed to reproduce this behavior.

## Model B: Coarse Tree

Model B is the preferred simple embedded implementation path. The exported tree has 5 nodes and max depth 2. It can be implemented as nested comparisons or a static node table. Thresholds are raw feature thresholds in the shared feature order.

## Items To Validate In The ESP32 Repo Later

- Floating-point parity between Python and ESP32 for all golden vectors.
- Feature extraction parity from raw weld logs or sensor stream to the 22-feature schema.
- RAM/flash budget for Model A before committing to a literal KNN implementation.
- Final policy for combining Model A and Model B outputs; that policy is intentionally not designed here.
