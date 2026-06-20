"""
Reusable feature extraction routines for weld LOADCELL signals.

This module centralises the computation of time-domain, FFT, and CWT-based
descriptors so that both the data preparation and modelling scripts can pick
and choose which feature families to include.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable, Dict, Iterable, Mapping

import numpy as np
import pywt
from scipy import signal

EPSILON = 1e-12
FFT_PAD_LENGTH = 4096


@dataclass(frozen=True)
class FeatureGroup:
    """Container describing a feature family and the callable that produces it."""

    name: str
    extractor: Callable[[np.ndarray, np.ndarray], Dict[str, float]]
    description: str


def compute_time_domain_features(load_values: np.ndarray, time_values: np.ndarray) -> Dict[str, float]:
    """Replicates the original eight time-domain statistics + area."""
    mean_val = float(np.mean(load_values))
    rms_val = float(np.sqrt(np.mean(np.square(load_values))))
    std_val = float(np.std(load_values, ddof=1))
    peak_val = float(np.max(np.abs(load_values)))
    mean_abs = float(np.mean(np.abs(load_values)))
    mean_sqrt = float(np.mean(np.sqrt(np.abs(load_values))))

    shape_factor = float(rms_val / mean_abs) if mean_abs else float("nan")
    crest_factor = float(peak_val / rms_val) if rms_val else float("nan")
    clearance_factor = float(peak_val / (mean_sqrt**2)) if mean_sqrt else float("nan")
    impulse_factor = float(peak_val / mean_abs) if mean_abs else float("nan")

    return {
        "Mean": mean_val,
        "RMS": rms_val,
        "StandardDeviation": std_val,
        "ShapeFactor": shape_factor,
        "PeakValue": peak_val,
        "CrestFactor": crest_factor,
        "ClearanceFactor": clearance_factor,
        "ImpulseFactor": impulse_factor,
    }


def compute_fft_features(load_values: np.ndarray, time_values: np.ndarray) -> Dict[str, float]:
    """
    Frequency-domain descriptors derived from the LOADCELL signal.

    Produces dominant frequency, spectral centroid/spread, bandwidth, and flatness.
    """
    # Remove DC component before FFT.
    demeaned = load_values - np.mean(load_values)
    time_diffs = np.diff(time_values)
    mean_dt = float(np.mean(time_diffs)) if len(time_diffs) else 1.0
    if mean_dt <= 0:
        mean_dt = 1.0

    # Zero-pad or truncate to target FFT length to align spectra.
    if len(demeaned) < FFT_PAD_LENGTH:
        pad_width = FFT_PAD_LENGTH - len(demeaned)
        demeaned = np.pad(demeaned, (0, pad_width))
    elif len(demeaned) > FFT_PAD_LENGTH:
        demeaned = demeaned[:FFT_PAD_LENGTH]

    if len(time_values) < FFT_PAD_LENGTH:
        pad_width = FFT_PAD_LENGTH - len(time_values)
        time_values = np.pad(time_values, (0, pad_width), mode="edge")
    elif len(time_values) > FFT_PAD_LENGTH:
        time_values = time_values[:FFT_PAD_LENGTH]

    time_diffs = np.diff(time_values)
    mean_dt = float(np.mean(time_diffs)) if len(time_diffs) else mean_dt
    if mean_dt <= 0:
        mean_dt = 1.0

    freqs = np.fft.rfftfreq(len(demeaned), d=mean_dt)
    spectrum = np.fft.rfft(demeaned)
    power = np.abs(spectrum) ** 2

    if power.size == 0:
        return {
            "FFT_DominantFreq": float("nan"),
            "FFT_SpectralCentroid": float("nan"),
            "FFT_SpectralSpread": float("nan"),
            "FFT_FrequencyBandwidth": float("nan"),
            "FFT_SpectralFlatness": float("nan"),
        }

    dominant_idx = int(np.argmax(power))
    dominant_freq = float(freqs[dominant_idx])

    power_sum = float(np.sum(power))
    centroid = float(np.sum(freqs * power) / power_sum) if power_sum else float("nan")
    spread = float(np.sqrt(np.sum(((freqs - centroid) ** 2) * power) / power_sum)) if power_sum else float("nan")

    # Frequency bandwidth at half power (–3 dB)
    half_power = power[dominant_idx] / 2.0
    above_half = np.where(power >= half_power)[0]
    if above_half.size:
        bandwidth = float(freqs[above_half[-1]] - freqs[above_half[0]])
    else:
        bandwidth = 0.0

    spectral_flatness = float(
        np.exp(np.mean(np.log(power + EPSILON))) / (np.mean(power) + EPSILON)
    )

    return {
        "FFT_DominantFreq": dominant_freq,
        "FFT_SpectralCentroid": centroid,
        "FFT_SpectralSpread": spread,
        "FFT_FrequencyBandwidth": bandwidth,
        "FFT_SpectralFlatness": spectral_flatness,
    }


def compute_cwt_features(load_values: np.ndarray, time_values: np.ndarray) -> Dict[str, float]:
    """
    Continuous Wavelet Transform-based descriptors.

    Extracts energy distribution across scales, dominant scale, and entropy.
    """
    # Use Morlet wavelet with a fixed set of scales.
    scales = np.array([1, 2, 4, 8, 16, 32], dtype=float)
    coefficients, _ = pywt.cwt(load_values, scales, "morl")
    power = np.abs(coefficients) ** 2

    scale_energies = np.sum(power, axis=1)
    total_energy = float(np.sum(scale_energies))
    if total_energy == 0:
        return {
            "CWT_TotalEnergy": 0.0,
            "CWT_DominantScale": float("nan"),
            "CWT_EnergyEntropy": float("nan"),
            "CWT_MaxScaleEnergy": 0.0,
            "CWT_MinScaleEnergy": 0.0,
        }

    dominant_scale = float(scales[int(np.argmax(scale_energies))])
    norm_energies = scale_energies / total_energy
    entropy = float(-np.sum(norm_energies * np.log(norm_energies + EPSILON)))

    return {
        "CWT_TotalEnergy": total_energy,
        "CWT_DominantScale": dominant_scale,
        "CWT_EnergyEntropy": entropy,
        "CWT_MaxScaleEnergy": float(np.max(scale_energies)),
        "CWT_MinScaleEnergy": float(np.min(scale_energies)),
    }


FEATURE_GROUPS: Mapping[str, FeatureGroup] = {
    "time": FeatureGroup(
        name="time",
        extractor=compute_time_domain_features,
        description="Original eight time-domain statistics.",
    ),
    "fft": FeatureGroup(
        name="fft",
        extractor=compute_fft_features,
        description="Frequency-domain descriptors from the FFT magnitude spectrum.",
    ),
    "cwt": FeatureGroup(
        name="cwt",
        extractor=compute_cwt_features,
        description="Continuous wavelet transform energy-based descriptors.",
    ),
}


def build_feature_vector(
    load_values: np.ndarray,
    time_values: np.ndarray,
    selected_groups: Iterable[str],
) -> Dict[str, float]:
    """Aggregates selected feature families into a single dictionary."""
    features: Dict[str, float] = {}
    for group_name in selected_groups:
        group = FEATURE_GROUPS.get(group_name)
        if not group:
            raise ValueError(f"Unknown feature group: {group_name}")
        features.update(group.extractor(load_values, time_values))
    return features
