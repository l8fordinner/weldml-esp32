"""
Parse Friction Stir Spot Welding (FSJ) log files, extract weld force features,
and generate a clean dataset ready for model training.
"""

from __future__ import annotations

import argparse
import io
import re
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable, List, Sequence

import numpy as np
import pandas as pd

try:
    from .feature_extraction import FEATURE_GROUPS, build_feature_vector
except ImportError:  # pragma: no cover - supports direct script execution
    from feature_extraction import FEATURE_GROUPS, build_feature_vector

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
DEFAULT_OUTPUT = REPO_ROOT / "data_processed" / "WSU_Tm_Dat.csv"

# fmt: off
FILE_SETS: dict[str, dict[str, tuple[str, ...]]] = {
    "legacy": {
        "IF": (
            "l041.fsj", "l042.fsj", "l043.fsj", "l044.fsj", "l045.fsj", "l046.fsj", "l047.fsj",
            "l055.fsj", "l056.fsj", "l057.fsj", "l068.fsj", "l069.fsj", "l070.fsj", "l071.fsj",
            "l072.fsj", "l073.fsj", "l074.fsj", "l075.fsj", "l076.fsj",
        ),
        "NP": (
            "l036.fsj", "l037.fsj", "l049.fsj", "l050.fsj", "l051.fsj", "l052.fsj",
            "l058.fsj", "l059.fsj", "l060.fsj", "l061.fsj", "l063.fsj", "l065.fsj",
            "l066.fsj", "l067.fsj", "l080.fsj", "l081.fsj", "l082.fsj", "l084.fsj",
            "l085.fsj",
        ),
    },
    "controlled_gap": {
        "NP": ("l313.fsj", "l314.fsj"),
        "IF": ("l315.fsj", "l316.fsj", "l317.fsj", "l318.fsj", "l319.fsj", "l320.fsj"),
    },
}
# fmt: on

BINARY_LABEL_MAP: dict[str, int] = {"IF": 1, "NP": 0}
POSITION_COLUMNS: tuple[str, ...] = (
    "POS7",
    "POSITION",
    "S.POS.M",
    "S.POS.LVDT",
    "POS9",
)
SPOSM_NORMALIZED_NAME = "SPOSM"
SEGMENT_START_MODES: tuple[str, ...] = (
    "sposm_ge_zero",
    "sposm_eq_zero",
    "stage2_sposm_gt0",
)

NUMERIC_LINE_RE = re.compile(r"^\s*[-+]?\d")
ROTATE_RE = re.compile(r"ROTATE\s*=\s*([0-9]+(?:\.[0-9]+)?)", re.IGNORECASE)


@dataclass
class WeldSample:
    """Container for processed sample metadata and computed features."""

    file_name: str
    failure_mode: str
    label: int
    rotation_speed: float
    sample_timestamp: str
    sample_date: str
    features: dict[str, float]

    def as_row(self) -> dict[str, float | str]:
        row: dict[str, float | str] = {
            "File": self.file_name,
            "FailureMode": self.failure_mode,
            "Label": self.label,
            "RotationSpeed": self.rotation_speed,
            "SampleTimestamp": self.sample_timestamp,
            "SampleDate": self.sample_date,
        }
        row.update(self.features)
        return row


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Extract weld force features from FSJ log files.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--input-dir",
        type=Path,
        required=True,
        help="Directory containing the raw .fsj files.",
    )
    parser.add_argument(
        "--output-csv",
        type=Path,
        default=DEFAULT_OUTPUT,
        help="Destination CSV path for the aggregated feature table.",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Allow overwriting an existing output CSV.",
    )
    parser.add_argument(
        "--feature-groups",
        nargs="+",
        choices=sorted(FEATURE_GROUPS.keys()),
        default=["time"],
        help=(
            "Feature groups to compute (default: time). "
            "Combine multiple groups, e.g. --feature-groups time fft."
        ),
    )
    parser.add_argument(
        "--file-set",
        nargs="+",
        choices=sorted(FILE_SETS.keys()),
        default=["legacy"],
        help=(
            "Named bundle of weld files to process. Provide multiple values to concatenate sets. "
            "Defaults to the original 38-sample legacy set."
        ),
    )
    parser.add_argument(
        "--segment-start",
        choices=SEGMENT_START_MODES,
        default="sposm_ge_zero",
        help=(
            "How to choose the segment start index. "
            "'sposm_ge_zero' matches MATLAB getRow(..., 0): first SPOSM >= 0; "
            "'sposm_eq_zero' matches the MATLAB-era SPOSM==0 anchor; "
            "'stage2_sposm_gt0' requires STAGE==2 and SPOSM>0."
        ),
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> None:
    args = parse_args(argv)
    failure_mode_map = _build_failure_mode_map(args.file_set)
    input_dir: Path = args.input_dir.expanduser().resolve()
    output_csv: Path = args.output_csv.expanduser()
    if not output_csv.is_absolute():
        output_csv = (REPO_ROOT / output_csv).resolve()

    if not input_dir.exists():
        raise FileNotFoundError(f"Input directory does not exist: {input_dir}")

    if output_csv.exists() and not args.overwrite:
        raise FileExistsError(
            f"Output file already exists: {output_csv}. "
            "Use --overwrite to replace it."
        )

    selected_groups = args.feature_groups
    target_files = sorted(failure_mode_map.keys())
    samples: list[WeldSample] = []
    failures: list[str] = []

    for file_name in target_files:
        file_path = input_dir / file_name
        try:
            failure_mode = failure_mode_map[file_name]
            label = BINARY_LABEL_MAP[failure_mode]
            sample = process_file(
                file_path,
                failure_mode,
                label,
                selected_groups,
                args.segment_start,
            )
            samples.append(sample)
        except Exception as exc:  # noqa: BLE001 - surface parsing issues to the user
            failures.append(f"{file_name}: {exc}")

    if failures:
        raise RuntimeError(
            "Failed to parse some files:\n" + "\n".join(failures)
        )

    rows = [sample.as_row() for sample in samples]
    df = pd.DataFrame(rows)

    # Arrange columns for readability.
    metadata_cols = [
        "File",
        "FailureMode",
        "Label",
        "RotationSpeed",
        "SampleTimestamp",
        "SampleDate",
    ]
    feature_cols = sorted(set(df.columns) - set(metadata_cols))
    df = df[metadata_cols + feature_cols]

    output_csv.parent.mkdir(parents=True, exist_ok=True)
    df.to_csv(output_csv, index=False)

    print(f"Wrote {len(df)} samples to {output_csv}")


def process_file(
    path: Path,
    failure_mode: str,
    label: int,
    feature_groups: Sequence[str],
    segment_start_mode: str,
) -> WeldSample:
    if not path.exists():
        raise FileNotFoundError(f"File not found: {path}")

    lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
    header_idx = _locate_header_line(lines)
    column_names = _extract_column_names(lines[header_idx])
    data_frame = _load_numeric_table(lines[header_idx + 1 :], column_names)

    if "STAGE" not in data_frame or "LOADCELL" not in data_frame:
        raise ValueError("Required columns STAGE and LOADCELL not present.")

    rotation_speed = _extract_rotation_speed(lines)
    features = _compute_features(data_frame, feature_groups, segment_start_mode)
    timestamp_iso, date_str = _extract_file_timestamp(path)

    return WeldSample(
        file_name=path.name,
        failure_mode=failure_mode,
        label=label,
        rotation_speed=rotation_speed,
        sample_timestamp=timestamp_iso,
        sample_date=date_str,
        features=features,
    )


def _locate_header_line(lines: Sequence[str]) -> int:
    for idx, line in enumerate(lines):
        if line.strip().startswith("TIME "):
            return idx
    raise ValueError("Unable to locate TIME column header.")


def _extract_column_names(header_line: str) -> List[str]:
    cols = re.split(r"\s+", header_line.strip())
    if len(cols) < 2:
        raise ValueError("Header line did not yield column names.")
    return cols


def _load_numeric_table(
    data_lines: Sequence[str], column_names: Sequence[str]
) -> pd.DataFrame:
    numeric_lines = []
    for line in data_lines:
        if NUMERIC_LINE_RE.match(line):
            numeric_lines.append(line)
        elif line.strip():  # Stop once we hit footer text.
            break

    if not numeric_lines:
        raise ValueError("No numeric data found below header.")

    buffer = io.StringIO("\n".join(numeric_lines))
    df = pd.read_csv(buffer, delim_whitespace=True, names=column_names)
    return df


def _extract_rotation_speed(lines: Iterable[str]) -> float:
    for line in lines:
        match = ROTATE_RE.search(line)
        if match:
            return float(match.group(1))
    raise ValueError("Rotation speed not found in footer.")


def _compute_features(
    df: pd.DataFrame, feature_groups: Sequence[str], segment_start_mode: str
) -> dict[str, float]:
    df_numeric = df.copy()
    for col in ("STAGE", "LOADCELL", "TIME"):
        if col not in df_numeric.columns:
            raise ValueError(f"Expected column {col} missing from data.")
    position_column = _select_position_column(df_numeric.columns)
    has_position = position_column is not None

    df_numeric["STAGE"] = pd.to_numeric(df_numeric["STAGE"], errors="coerce")
    df_numeric["LOADCELL"] = pd.to_numeric(df_numeric["LOADCELL"], errors="coerce")
    df_numeric["TIME"] = pd.to_numeric(df_numeric["TIME"], errors="coerce")
    if has_position:
        df_numeric["POSITION"] = pd.to_numeric(
            df_numeric[position_column], errors="coerce"
        )
    else:
        df_numeric["POSITION"] = np.nan

    if (
        df_numeric["STAGE"].isna().all()
        or df_numeric["LOADCELL"].isna().all()
    ):
        raise ValueError("Stage or load data could not be converted to numeric.")

    stage_series = df_numeric["STAGE"].astype(pd.Int64Dtype())
    load_series = df_numeric["LOADCELL"]
    time_series = df_numeric["TIME"]
    position_series = df_numeric["POSITION"]

    stage3_indices = stage_series[stage_series == 3].index
    if stage3_indices.empty:
        raise ValueError("No stage 3 samples located in file.")

    start_idx = _find_start_index(df_numeric, stage_series, segment_start_mode)
    end_idx = int(stage3_indices[-1])
    if end_idx <= start_idx:
        raise ValueError("Stage 3 samples occur before stage 2 segment.")

    load_segment = load_series.loc[start_idx : end_idx].dropna()
    time_segment = time_series.loc[start_idx : end_idx].dropna()

    if load_segment.empty or time_segment.empty:
        raise ValueError("Selected load or time segment is empty.")

    load_values = load_segment.to_numpy()
    time_values = time_segment.to_numpy()

    if has_position:
        extra_features = _compute_stage_metrics(
            df_numeric, stage_series, time_series, load_series, position_series
        )
    else:
        extra_features = {
            "PlungeVelocity": 0.0,
            "MinPositionStage3": 0.0,
            "MaxForceBelow3mm": 0.0,
        }
    features = build_feature_vector(load_values, time_values, feature_groups)
    features.update(extra_features)
    return features


def _compute_stage_metrics(
    df: pd.DataFrame,
    stage_series: pd.Series,
    time_series: pd.Series,
    load_series: pd.Series,
    position_series: pd.Series,
) -> dict[str, float]:
    stage2_df = df[stage_series == 2].dropna(subset=["TIME", "POSITION"])
    stage3_df = df[(stage_series == 3) & (position_series < 3.0)].dropna(subset=["POSITION"])

    t_30 = _interpolated_time_at_position(stage2_df, 3.0)
    t_25 = _interpolated_time_at_position(stage2_df, 2.5)
    if t_30 is not None and t_25 is not None and t_25 != t_30:
        plunge_velocity = 0.5 / abs(t_25 - t_30)
    else:
        plunge_velocity = None

    if not stage3_df.empty:
        min_position_stage3 = float(stage3_df["POSITION"].min())
    else:
        min_position_stage3 = None

    mask_below3 = (position_series < 3.0) & (stage_series.isin([2, 3]))
    subset_force = load_series[mask_below3].dropna()
    if not subset_force.empty:
        max_force = float(subset_force.max())
    else:
        max_force = None

    return {
        "PlungeVelocity": _safe_numeric(plunge_velocity),
        "MinPositionStage3": _safe_numeric(min_position_stage3),
        "MaxForceBelow3mm": _safe_numeric(max_force),
    }


def _interpolated_time_at_position(df: pd.DataFrame, target: float) -> float | None:
    if df.empty:
        return None
    positions = df["POSITION"].to_numpy(dtype=float)
    times = df["TIME"].to_numpy(dtype=float)
    for idx in range(len(positions) - 1):
        p1, p2 = positions[idx], positions[idx + 1]
        t1, t2 = times[idx], times[idx + 1]
        if np.isnan(p1) or np.isnan(p2) or np.isnan(t1) or np.isnan(t2):
            continue
        if p1 == target:
            return t1
        if p2 == target:
            return t2
        if (p1 - target) * (p2 - target) < 0:
            ratio = (target - p1) / (p2 - p1)
            return t1 + ratio * (t2 - t1)
    # fall back to closest sample
    valid_idx = np.where(~np.isnan(positions) & ~np.isnan(times))[0]
    if valid_idx.size == 0:
        return None
    idx_closest = valid_idx[np.argmin(np.abs(positions[valid_idx] - target))]
    return times[idx_closest]


def _safe_numeric(value: float | None, default: float = 0.0) -> float:
    if value is None:
        return default
    try:
        if np.isnan(value) or np.isinf(value):
            return default
    except TypeError:
        pass
    return float(value)


def _select_position_column(columns: Sequence[str]) -> str | None:
    for candidate in POSITION_COLUMNS:
        if candidate in columns:
            return candidate
    return None


def _normalize_column_name(name: str) -> str:
    return re.sub(r"[^A-Z0-9]", "", name.upper())


def _resolve_sposm_column(df: pd.DataFrame) -> str:
    sposm_column = None
    for column in df.columns:
        if _normalize_column_name(column) == SPOSM_NORMALIZED_NAME:
            sposm_column = column
            break

    if sposm_column is None:
        raise ValueError(
            "Required SPOSM column not found (expected SPOSM or S.POS.M style name)."
        )
    return sposm_column


def _find_start_index(
    df: pd.DataFrame,
    stage_series: pd.Series,
    mode: str,
) -> int:
    sposm_column = _resolve_sposm_column(df)
    sposm_values = pd.to_numeric(df[sposm_column], errors="coerce")
    sposm_array = sposm_values.to_numpy(dtype=float)

    if mode == "sposm_ge_zero":
        ge_zero_mask = sposm_array >= 0.0
        ge_zero_indices = sposm_values.index[ge_zero_mask]
        if ge_zero_indices.empty:
            raise ValueError("No SPOSM >= 0 samples located in file.")
        return int(ge_zero_indices[0])

    if mode == "sposm_eq_zero":
        zero_mask = np.isclose(
            sposm_array,
            0.0,
            atol=1e-9,
            rtol=0.0,
        )
        zero_indices = sposm_values.index[zero_mask]
        if zero_indices.empty:
            raise ValueError("No SPOSM == 0 samples located in file.")
        return int(zero_indices[0])

    if mode == "stage2_sposm_gt0":
        stage2_spos_mask = (stage_series == 2) & (sposm_values > 0)
        stage2_spos_indices = stage_series[stage2_spos_mask].index
        if stage2_spos_indices.empty:
            raise ValueError("No samples where STAGE == 2 and SPOSM > 0.")
        return int(stage2_spos_indices[0])

    raise ValueError(f"Unsupported segment start mode: {mode}")


def _find_first_sposm_zero_index(df: pd.DataFrame) -> int:
    """Backward-compatible helper retained for transcript traceability."""
    sposm_column = _resolve_sposm_column(df)
    sposm_values = pd.to_numeric(df[sposm_column], errors="coerce")
    zero_mask = np.isclose(sposm_values.to_numpy(dtype=float), 0.0, atol=1e-9, rtol=0.0)
    zero_indices = sposm_values.index[zero_mask]
    if zero_indices.empty:
        raise ValueError("No SPOSM == 0 samples located in file.")
    return int(zero_indices[0])


def _extract_file_timestamp(path: Path) -> tuple[str, str]:
    """Return ISO timestamp and date string derived from the file's modification time."""
    stat = path.stat()
    dt = datetime.fromtimestamp(stat.st_mtime, tz=timezone.utc)
    timestamp = dt.isoformat()
    date_str = dt.date().isoformat()
    return timestamp, date_str


def _build_failure_mode_map(selected_sets: Sequence[str]) -> dict[str, str]:
    failure_map: dict[str, str] = {}
    for set_name in selected_sets:
        config = FILE_SETS.get(set_name)
        if not config:
            raise ValueError(f"Unknown file set: {set_name}")
        for failure_mode, files in config.items():
            for file_name in files:
                existing = failure_map.get(file_name)
                if existing and existing != failure_mode:
                    raise ValueError(
                        f"File {file_name} is assigned to both {existing} and {failure_mode}."
                    )
                failure_map[file_name] = failure_mode
    if not failure_map:
        raise ValueError("Selected file sets did not supply any files.")
    return failure_map


if __name__ == "__main__":
    main()
