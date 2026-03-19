from __future__ import annotations

import gzip
import hashlib
import json
import random
import struct
from collections import defaultdict
from pathlib import Path

import numpy as np
import torch
import torch.nn.functional as F
from torch.utils.data import Dataset

from python.train.dataset import resolve_dataset_paths

# Max board dimensions from upstream referee GridMaker.java:
# height in [10, 24], width = round(height * 1.8) forced even → max 44.
MAX_GRID_CHANNELS = 19
MAX_GRID_HEIGHT = 24
MAX_GRID_WIDTH = 44


def _pad_grid(grid: torch.Tensor) -> torch.Tensor:
    """Zero-pad a (C, H, W) grid tensor to (MAX_GRID_CHANNELS, MAX_GRID_HEIGHT, MAX_GRID_WIDTH)."""
    _, h, w = grid.shape
    if h < MAX_GRID_HEIGHT or w < MAX_GRID_WIDTH:
        grid = F.pad(grid, (0, MAX_GRID_WIDTH - w, 0, MAX_GRID_HEIGHT - h))
    return grid


class HybridSelfPlayDataset(Dataset[dict[str, torch.Tensor]]):
    def __init__(self, path: str | Path, max_samples: int = 0) -> None:
        self.paths = resolve_dataset_paths(path)
        # Compact storage: numpy arrays instead of Python dicts (~80KB vs ~300KB per row)
        self._grids: list[np.ndarray] = []
        self._scalars: list[np.ndarray] = []
        self._values: list[float] = []
        self._policy_targets: list[list[int]] = []
        self._weights: list[float] = []
        # Lightweight metadata for dedup/split (kept in self.rows for compatibility)
        self.rows: list[dict] = []
        n_loaded = 0
        for dataset_path in self.paths:
            if dataset_path.suffix == ".gz":
                handle = gzip.open(dataset_path, "rt", encoding="utf-8")
            else:
                handle = dataset_path.open("r", encoding="utf-8")
            with handle:
                for line in handle:
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        row = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    if "hybrid_grid" not in row:
                        continue
                    try:
                        grid = np.array(row["hybrid_grid"], dtype=np.float32)
                    except (ValueError, RuntimeError):
                        continue
                    self._grids.append(grid)
                    self._scalars.append(np.array(row.get("scalars", []), dtype=np.float32))
                    self._values.append(float(row.get("value", 0.0)))
                    self._policy_targets.append(row.get("policy_targets", [-100, -100, -100, -100]))
                    self._weights.append(float(row.get("weight", 1.0)))
                    # Store only lightweight metadata
                    self.rows.append({
                        "encoded_view_hash": row.get("encoded_view_hash"),
                        "seed": row.get("seed", 0),
                        "game_id": row.get("game_id", "game-0"),
                    })
                    self._on_row_loaded(row)
                    n_loaded += 1
                    if n_loaded % 1_000_000 == 0:
                        print(f"[dataset] Loaded {n_loaded/1e6:.1f}M rows...", flush=True)
                    if max_samples and n_loaded >= max_samples:
                        break
            if max_samples and n_loaded >= max_samples:
                break
        if not self.rows:
            raise ValueError(f"hybrid dataset is empty: {path}")

    def _on_row_loaded(self, row: dict) -> None:
        """Hook for subclasses to store extra per-row data during loading."""
        pass

    def __len__(self) -> int:
        return len(self.rows)

    def __getitem__(self, index: int) -> dict[str, torch.Tensor]:
        grid = _pad_grid(torch.from_numpy(self._grids[index]))
        return {
            "grid": grid,
            "scalars": torch.from_numpy(self._scalars[index]),
            "value": torch.tensor([self._values[index]], dtype=torch.float32),
            "policy_targets": torch.tensor(self._policy_targets[index], dtype=torch.long),
            "weight": torch.tensor([self._weights[index]], dtype=torch.float32),
        }


def dedup_rows(rows: list[dict]) -> list[dict]:
    """Legacy dedup for plain row dicts. Returns deduped list."""
    deduped: list[dict] = []
    seen: set[str] = set()
    for row in rows:
        key = str(row.get("encoded_view_hash", f"row-{len(deduped)}"))
        if key in seen:
            continue
        seen.add(key)
        deduped.append(row)
    return deduped


def dedup_dataset(dataset: "HybridSelfPlayDataset") -> None:
    """In-place dedup across all compact storage arrays."""
    keep: list[int] = []
    seen: set[str] = set()
    for i, row in enumerate(dataset.rows):
        key = str(row.get("encoded_view_hash", f"row-{i}"))
        if key in seen:
            continue
        seen.add(key)
        keep.append(i)
    if len(keep) == len(dataset.rows):
        return  # no duplicates
    dataset.rows = [dataset.rows[i] for i in keep]
    dataset._grids = [dataset._grids[i] for i in keep]
    dataset._scalars = [dataset._scalars[i] for i in keep]
    dataset._values = [dataset._values[i] for i in keep]
    dataset._policy_targets = [dataset._policy_targets[i] for i in keep]
    dataset._weights = [dataset._weights[i] for i in keep]
    # Handle distill subclass fields
    if hasattr(dataset, "_teacher_policy_logits"):
        dataset._teacher_policy_logits = [dataset._teacher_policy_logits[i] for i in keep]
    if hasattr(dataset, "_teacher_values"):
        dataset._teacher_values = [dataset._teacher_values[i] for i in keep]


class HybridDistillDataset(HybridSelfPlayDataset):
    """Extends HybridSelfPlayDataset with teacher soft targets for distillation."""

    def __init__(self, path: str | Path, max_samples: int = 0) -> None:
        self._teacher_policy_logits: list[np.ndarray | None] = []
        self._teacher_values: list[float | None] = []
        super().__init__(path, max_samples)

    def _on_row_loaded(self, row: dict) -> None:
        if "teacher_policy_logits" in row:
            self._teacher_policy_logits.append(np.array(row["teacher_policy_logits"], dtype=np.float32))
        else:
            self._teacher_policy_logits.append(None)
        if "teacher_value" in row:
            self._teacher_values.append(float(row["teacher_value"]))
        else:
            self._teacher_values.append(None)

    def __getitem__(self, index: int) -> dict[str, torch.Tensor]:
        item = super().__getitem__(index)
        tpl = self._teacher_policy_logits[index]
        if tpl is not None:
            item["teacher_policy_logits"] = torch.from_numpy(tpl)
        tv = self._teacher_values[index]
        if tv is not None:
            item["teacher_value"] = torch.tensor([tv], dtype=torch.float32)
        return item


class BitpackedDataset(Dataset):
    """Memory-mapped dataset from sharded bitpacked binary files.

    Supports both format_version 1 (monolithic data.bin) and 2 (sharded shard_NNN_data.bin).
    Uses ~360MB RAM regardless of dataset size (metadata only). Grid data is memory-mapped.
    """

    # Layout constants (must match pack_dataset.py)
    GRID_PACKED_BYTES = 2508
    DATA_ROW_BYTES = 2528
    META_ROW_BYTES = 16

    def __init__(self, path: str | Path) -> None:
        path = Path(path)
        with open(path / "header.json", "r") as f:
            self._header = json.load(f)

        self._num_rows = self._header["num_rows"]
        version = self._header.get("format_version", 1)

        if version >= 2 and "shards" in self._header:
            self._init_sharded(path)
        else:
            self._init_monolithic(path)

    def _init_monolithic(self, path: Path) -> None:
        """Load format v1: single data.bin + meta.bin."""
        self._data = np.memmap(
            path / "data.bin", dtype=np.uint8, mode="r",
            shape=(self._num_rows, self.DATA_ROW_BYTES),
        )
        meta_raw = np.memmap(
            path / "meta.bin", dtype=np.uint8, mode="r",
            shape=(self._num_rows, self.META_ROW_BYTES),
        )
        self._load_meta(meta_raw)

    def _init_sharded(self, path: Path) -> None:
        """Load format v2: sharded shard_NNN_data.bin + shard_NNN_meta.bin."""
        shards = self._header["shards"]
        data_mmaps = []
        meta_parts = []
        for shard in shards:
            n = shard["num_rows"]
            data_mmaps.append(np.memmap(
                path / f"{shard['id']}_data.bin", dtype=np.uint8, mode="r",
                shape=(n, self.DATA_ROW_BYTES),
            ))
            meta_parts.append(np.memmap(
                path / f"{shard['id']}_meta.bin", dtype=np.uint8, mode="r",
                shape=(n, self.META_ROW_BYTES),
            ))
        # Build shard offset table for O(1) index lookup
        self._shard_mmaps = data_mmaps
        self._shard_offsets = []  # (start_row, end_row) per shard
        self._shard_starts = []  # start row per shard (for bisect)
        offset = 0
        for mmap in data_mmaps:
            n = mmap.shape[0]
            self._shard_starts.append(offset)
            self._shard_offsets.append((offset, offset + n))
            offset += n
        # Concatenate meta into single arrays
        meta_raw = np.concatenate(meta_parts, axis=0)
        self._load_meta(meta_raw)

    def _load_meta(self, meta_raw: np.ndarray) -> None:
        meta_structured = np.frombuffer(
            meta_raw, dtype=np.dtype([
                ("hash", "<u8"), ("game_id_hash", "<u4"), ("seed", "<i4"),
            ]),
        )
        self._hashes = np.array(meta_structured["hash"])
        self._seeds = np.array(meta_structured["seed"])
        self._game_id_hashes = np.array(meta_structured["game_id_hash"])

    def __len__(self) -> int:
        return self._num_rows

    def _get_row(self, index: int) -> np.ndarray:
        """Get raw row bytes, handling both monolithic and sharded storage."""
        if hasattr(self, "_shard_mmaps"):
            from bisect import bisect_right
            si = bisect_right(self._shard_starts, index) - 1
            return self._shard_mmaps[si][index - self._shard_starts[si]]
        return self._data[index]

    def __getitem__(self, index: int) -> dict[str, torch.Tensor]:
        row = self._get_row(index)

        # Grid: unpackbits → (19, 24, 44) float32
        grid_packed = row[:self.GRID_PACKED_BYTES]
        grid_bits = np.unpackbits(grid_packed)[: MAX_GRID_CHANNELS * MAX_GRID_HEIGHT * MAX_GRID_WIDTH]
        grid = grid_bits.reshape(MAX_GRID_CHANNELS, MAX_GRID_HEIGHT, MAX_GRID_WIDTH).astype(np.float32)

        # Scalars: float16[6] → float32
        scalars = np.frombuffer(
            row[self.GRID_PACKED_BYTES : self.GRID_PACKED_BYTES + 12].tobytes(),
            dtype=np.float16,
        ).astype(np.float32)

        # Value: float16 → float32
        value = np.frombuffer(
            row[self.GRID_PACKED_BYTES + 12 : self.GRID_PACKED_BYTES + 14].tobytes(),
            dtype=np.float16,
        ).astype(np.float32)

        # Policy targets: int8[4] → long
        policy = np.frombuffer(
            row[self.GRID_PACKED_BYTES + 14 : self.GRID_PACKED_BYTES + 18].tobytes(),
            dtype=np.int8,
        ).astype(np.int64)

        # Weight: float16 → float32
        weight = np.frombuffer(
            row[self.GRID_PACKED_BYTES + 18 : self.GRID_PACKED_BYTES + 20].tobytes(),
            dtype=np.float16,
        ).astype(np.float32)

        return {
            "grid": torch.from_numpy(grid),
            "scalars": torch.from_numpy(scalars),
            "value": torch.from_numpy(value),
            "policy_targets": torch.from_numpy(policy),
            "weight": torch.from_numpy(weight),
        }


class BitpackedDistillDataset(Dataset):
    """Wraps BitpackedDataset with teacher predictions for distillation.

    Loads teacher_predictions.npz and accesses base bitpacked rows via mmap.
    On local SSD, mmap random access is fast (~100K IOPS). Use preload=True
    only for slow network mounts (e.g. R2 CloudBucketMount).
    """

    def __init__(
        self,
        predictions_dir: str | Path,
        base_path_override: str | Path | None = None,
        preload: bool = True,
        max_samples: int = 0,
        chunk_offset: int = 0,
    ) -> None:
        import time as _time
        predictions_dir = Path(predictions_dir)
        npz = np.load(predictions_dir / "teacher_predictions.npz")
        meta = json.load(open(predictions_dir / "teacher_predictions_meta.json"))

        indices = npz["indices"]  # into the base bitpacked dataset
        teacher_policy = npz["teacher_policy_logits"]  # (N, 4, 5) f16
        teacher_values = npz["teacher_values"]  # (N,) f16

        # Sub-sample if requested (strided with offset for chunk cycling)
        if max_samples and max_samples < len(indices):
            step = len(indices) // max_samples
            sel = np.arange(chunk_offset, len(indices), step)[:max_samples]
            indices = indices[sel]
            teacher_policy = teacher_policy[sel]
            teacher_values = teacher_values[sel]
            n_chunks = step  # total chunks available
            print(f"[distill-dataset] Sub-sampled to {len(indices):,} rows "
                  f"(step={step}, chunk {chunk_offset+1}/{n_chunks})", flush=True)

        self._indices = indices
        self._teacher_policy = teacher_policy
        self._teacher_values = teacher_values
        self._num_rows = len(self._indices)

        # Load the base bitpacked dataset for metadata + raw data access
        base_path = Path(base_path_override) if base_path_override else Path(meta["dataset_path"])
        self._base = BitpackedDataset(base_path)
        print(
            f"[distill-dataset] {self._num_rows:,} rows with teacher predictions "
            f"(base: {len(self._base):,} rows)",
            flush=True,
        )

        # Expose seeds/hashes for grouped splitting
        self._seeds = self._base._seeds[self._indices]
        self._game_id_hashes = self._base._game_id_hashes[self._indices]

        if preload:
            # Preload into RAM — SEQUENTIAL shard-by-shard for fast I/O
            print(f"[distill-dataset] Preloading {self._num_rows:,} raw rows into RAM...", flush=True)
            preload_start = _time.time()
            self._raw_rows = np.zeros((self._num_rows, BitpackedDataset.DATA_ROW_BYTES), dtype=np.uint8)
            # Sort by base index for sequential disk access, then scatter back
            order = np.argsort(self._indices)
            for count, i in enumerate(order):
                self._raw_rows[i] = self._base._get_row(int(self._indices[i]))
                if (count + 1) % 1_000_000 == 0:
                    elapsed = _time.time() - preload_start
                    rate = (count + 1) / elapsed
                    eta = (self._num_rows - count - 1) / rate
                    print(
                        f"[distill-dataset] Preloaded {count+1:,}/{self._num_rows:,} "
                        f"({elapsed:.0f}s, {rate:.0f} rows/s, ETA {eta:.0f}s)",
                        flush=True,
                    )
            preload_mb = self._raw_rows.nbytes / 1024 / 1024
            print(
                f"[distill-dataset] Preloaded in {_time.time()-preload_start:.0f}s "
                f"({preload_mb:.0f} MB in RAM)",
                flush=True,
            )
        else:
            self._raw_rows = None
            print(f"[distill-dataset] Using mmap (no preload) — {self._num_rows:,} rows", flush=True)

    def __len__(self) -> int:
        return self._num_rows

    def __getitem__(self, index: int) -> dict[str, torch.Tensor]:
        if self._raw_rows is not None:
            row = self._raw_rows[index]
        else:
            row = self._base._get_row(int(self._indices[index]))

        # Unpack (same logic as BitpackedDataset.__getitem__)
        grid_packed = row[:BitpackedDataset.GRID_PACKED_BYTES]
        grid_bits = np.unpackbits(grid_packed)[: MAX_GRID_CHANNELS * MAX_GRID_HEIGHT * MAX_GRID_WIDTH]
        grid = grid_bits.reshape(MAX_GRID_CHANNELS, MAX_GRID_HEIGHT, MAX_GRID_WIDTH).astype(np.float32)

        off = BitpackedDataset.GRID_PACKED_BYTES
        scalars = np.frombuffer(row[off:off + 12].tobytes(), dtype=np.float16).astype(np.float32)
        value = np.frombuffer(row[off + 12:off + 14].tobytes(), dtype=np.float16).astype(np.float32)
        policy = np.frombuffer(row[off + 14:off + 18].tobytes(), dtype=np.int8).astype(np.int64)
        weight = np.frombuffer(row[off + 18:off + 20].tobytes(), dtype=np.float16).astype(np.float32)

        return {
            "grid": torch.from_numpy(grid),
            "scalars": torch.from_numpy(scalars),
            "value": torch.from_numpy(value),
            "policy_targets": torch.from_numpy(policy),
            "weight": torch.from_numpy(weight),
            "teacher_policy_logits": torch.from_numpy(
                self._teacher_policy[index].astype(np.float32)
            ),
            "teacher_value": torch.tensor(
                [float(self._teacher_values[index])], dtype=torch.float32
            ),
        }


def grouped_split_indices_bitpacked(
    seeds: np.ndarray,
    game_id_hashes: np.ndarray,
    train_split: float,
    seed: int,
) -> tuple[list[int], list[int]]:
    """Vectorized grouped split — assigns each (seed, game_id_hash) group
    to train or valid based on a deterministic hash. No Python loops over 36M rows."""
    # Combine seed+game_id_hash into a single hash per row (vectorized)
    group_hash = seeds.astype(np.int64) * np.int64(2654435761) + game_id_hashes.astype(np.int64)
    group_hash = group_hash ^ np.int64(seed)  # incorporate split seed
    # Deterministic split: train if (hash % 1000) < (train_split * 1000)
    threshold = int(train_split * 1000)
    is_train = (np.abs(group_hash) % 1000) < threshold
    train_indices = np.where(is_train)[0].tolist()
    valid_indices = np.where(~is_train)[0].tolist()
    if not valid_indices and train_indices:
        valid_indices.append(train_indices.pop())
    if not train_indices and valid_indices:
        train_indices.append(valid_indices.pop())
    return train_indices, valid_indices


def grouped_split_indices(rows: list[dict], train_split: float, seed: int) -> tuple[list[int], list[int]]:
    grouped: dict[tuple[int, str], list[int]] = defaultdict(list)
    for index, row in enumerate(rows):
        grouped[(int(row.get("seed", 0)), str(row.get("game_id", "game-0")))].append(index)

    groups = list(grouped.items())
    rng = random.Random(seed)
    rng.shuffle(groups)

    target_train = max(1, int(len(rows) * train_split))
    train_indices: list[int] = []
    valid_indices: list[int] = []
    for _, indices in groups:
        bucket = train_indices if len(train_indices) < target_train else valid_indices
        bucket.extend(indices)
    if not valid_indices and train_indices:
        valid_indices.append(train_indices.pop())
    if not train_indices and valid_indices:
        train_indices.append(valid_indices.pop())
    return train_indices, valid_indices
