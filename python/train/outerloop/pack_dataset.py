#!/usr/bin/env python3
"""Pack JSONL self-play data into sharded bitpacked binary format for memory-mapped training.

Sharded format (in output-dir/):
  header.json                  — global metadata + shard list
  shard_000_data.bin           — rows 0..N  (N = rows_per_shard)
  shard_000_meta.bin
  shard_001_data.bin           — rows N..2N
  shard_001_meta.bin
  ...

Each data row is 2,528 bytes:
  [0..2508)    grid: np.packbits(hybrid_grid[19,24,44].flatten())
  [2508..2520) scalars: float16[6]
  [2520..2522) value: float16
  [2522..2526) policy_targets: int8[4]
  [2526..2528) weight: float16

Each meta row is 16 bytes:
  [0..8)   hash: uint64
  [8..12)  game_id_hash: uint32
  [12..16) seed: int32

Usage:
    # Full pack:
    python3 -m python.train.outerloop.pack_dataset \\
        --input data/selfplay/ --input data/selfplay_contabo/ \\
        --output-dir data/bitpacked --workers 12

    # Append new data (only writes new shards, skips known hashes):
    python3 -m python.train.outerloop.pack_dataset \\
        --input data/new_seeds/ --output-dir data/bitpacked --append --workers 12

    # Then sync to R2 (only uploads new shards):
    aws s3 sync data/bitpacked/ s3://snakebot-data/bitpacked/ --endpoint-url ...
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import struct
import sys
import time
from multiprocessing import Pool
from pathlib import Path

import numpy as np

# Must match dataset.py constants
MAX_GRID_CHANNELS = 19
MAX_GRID_HEIGHT = 24
MAX_GRID_WIDTH = 44

GRID_FLAT_BITS = MAX_GRID_CHANNELS * MAX_GRID_HEIGHT * MAX_GRID_WIDTH  # 20064
GRID_PACKED_BYTES = (GRID_FLAT_BITS + 7) // 8  # 2508
DATA_ROW_BYTES = GRID_PACKED_BYTES + 12 + 2 + 4 + 2  # 2528
META_ROW_BYTES = 16
DEFAULT_ROWS_PER_SHARD = 400_000  # ~1GB per shard


def _hash_to_uint64(hex_str: str) -> int:
    """Convert hex encoded_view_hash to uint64 by taking first 8 bytes."""
    raw = bytes.fromhex(hex_str[:16].ljust(16, "0"))
    return struct.unpack("<Q", raw[:8])[0]


def _game_id_hash(game_id: str) -> int:
    """Hash game_id string to uint32."""
    return struct.unpack("<I", hashlib.md5(game_id.encode()).digest()[:4])[0]


def pack_row(row: dict) -> tuple[bytes, bytes, int]:
    """Convert one JSON row to (data_bytes, meta_bytes, hash_uint64)."""
    # Grid: pad to (19, 24, 44), packbits
    grid = np.array(row["hybrid_grid"], dtype=np.uint8)
    c, h, w = grid.shape
    padded = np.zeros((MAX_GRID_CHANNELS, MAX_GRID_HEIGHT, MAX_GRID_WIDTH), dtype=np.uint8)
    padded[:c, :h, :w] = grid
    grid_packed = np.packbits(padded.flatten())
    assert len(grid_packed) == GRID_PACKED_BYTES

    # Scalars: 6 × float16
    scalars = np.array(row.get("scalars", [0.0] * 6), dtype=np.float16)
    if len(scalars) < 6:
        scalars = np.pad(scalars, (0, 6 - len(scalars)))

    # Value: float16
    value = np.float16(row.get("value", 0.0))

    # Policy targets: int8[4]
    policy = np.array(
        row.get("policy_targets", [-100, -100, -100, -100]), dtype=np.int8
    )

    # Weight: float16
    weight = np.float16(row.get("weight", 1.0))

    data_bytes = (
        grid_packed.tobytes()
        + scalars.tobytes()
        + value.tobytes()
        + policy.tobytes()
        + weight.tobytes()
    )
    assert len(data_bytes) == DATA_ROW_BYTES

    # Meta
    evh = str(row.get("encoded_view_hash", "0"))
    hash_val = _hash_to_uint64(evh)
    seed_val = int(row.get("seed", 0))
    gid_hash = _game_id_hash(str(row.get("game_id", "game-0")))

    meta_bytes = struct.pack("<QIi", hash_val, gid_hash, seed_val)
    assert len(meta_bytes) == META_ROW_BYTES

    return data_bytes, meta_bytes, hash_val


def _process_file(path_str: str) -> tuple[list[bytes], list[bytes], set[int], int, int]:
    """Process one JSONL(.gz) file. Returns (data_chunks, meta_chunks, hashes, kept, skipped)."""
    path = Path(path_str)
    data_chunks: list[bytes] = []
    meta_chunks: list[bytes] = []
    hashes: set[int] = set()
    skipped = 0

    if path.suffix == ".gz":
        handle = gzip.open(path, "rt", encoding="utf-8")
    else:
        handle = path.open("r", encoding="utf-8")

    with handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                skipped += 1
                continue
            if "hybrid_grid" not in row:
                skipped += 1
                continue
            try:
                data_b, meta_b, h = pack_row(row)
            except Exception:
                skipped += 1
                continue
            data_chunks.append(data_b)
            meta_chunks.append(meta_b)
            hashes.add(h)

    return data_chunks, meta_chunks, hashes, len(data_chunks), skipped


class _ShardWriter:
    """Writes rows into sharded data/meta files."""

    def __init__(self, output_dir: Path, rows_per_shard: int, start_shard: int = 0):
        self.output_dir = output_dir
        self.rows_per_shard = rows_per_shard
        self.shard_id = start_shard
        self.shard_rows = 0
        self.shards: list[dict] = []
        self._data_f = None
        self._meta_f = None
        self._open_shard()

    def _shard_name(self, shard_id: int) -> str:
        return f"shard_{shard_id:03d}"

    def _open_shard(self) -> None:
        name = self._shard_name(self.shard_id)
        self._data_f = open(self.output_dir / f"{name}_data.bin", "wb")
        self._meta_f = open(self.output_dir / f"{name}_meta.bin", "wb")
        self.shard_rows = 0

    def _close_shard(self) -> None:
        if self._data_f is not None:
            self._data_f.close()
            self._meta_f.close()
            if self.shard_rows > 0:
                self.shards.append({
                    "id": self._shard_name(self.shard_id),
                    "num_rows": self.shard_rows,
                })

    def write_row(self, data_bytes: bytes, meta_bytes: bytes) -> None:
        self._data_f.write(data_bytes)
        self._meta_f.write(meta_bytes)
        self.shard_rows += 1
        if self.shard_rows >= self.rows_per_shard:
            self._close_shard()
            self.shard_id += 1
            self._open_shard()

    def close(self) -> None:
        self._close_shard()
        # Remove empty last shard files if no rows written
        if self.shard_rows == 0:
            name = self._shard_name(self.shard_id)
            for suffix in ("_data.bin", "_meta.bin"):
                p = self.output_dir / f"{name}{suffix}"
                if p.exists() and p.stat().st_size == 0:
                    p.unlink()


def _load_existing_hashes(output_dir: Path) -> tuple[set[int], list[dict], int, set[str]]:
    """Load hashes from existing shards for append mode."""
    header_path = output_dir / "header.json"
    if not header_path.exists():
        return set(), [], 0, set()

    with open(header_path, "r") as f:
        header = json.load(f)

    shards = header.get("shards", [])
    processed_files = set(header.get("processed_files", []))
    global_hashes: set[int] = set()
    total_rows = 0

    for shard in shards:
        meta_path = output_dir / f"{shard['id']}_meta.bin"
        if not meta_path.exists():
            continue
        meta_raw = np.fromfile(meta_path, dtype=np.uint8)
        n_rows = shard["num_rows"]
        meta_structured = np.frombuffer(
            meta_raw[:n_rows * META_ROW_BYTES],
            dtype=np.dtype([("hash", "<u8"), ("game_id_hash", "<u4"), ("seed", "<i4")]),
        )
        for h in meta_structured["hash"]:
            global_hashes.add(int(h))
        total_rows += n_rows

    return global_hashes, shards, total_rows, processed_files


def main() -> None:
    parser = argparse.ArgumentParser(description="Pack JSONL self-play data to sharded bitpacked binary")
    parser.add_argument(
        "--input", action="append", required=True,
        help="Input path (file or directory). Can be specified multiple times.",
    )
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--workers", type=int, default=8)
    parser.add_argument("--rows-per-shard", type=int, default=DEFAULT_ROWS_PER_SHARD,
                        help=f"Rows per shard file (default {DEFAULT_ROWS_PER_SHARD}, ~1GB)")
    parser.add_argument("--append", action="store_true",
                        help="Append to existing shards (skips known hashes, writes new shards only)")
    args = parser.parse_args()

    # Resolve all input files
    input_files: list[Path] = []
    for inp in args.input:
        p = Path(inp)
        if p.is_dir():
            input_files.extend(sorted(p.glob("*.jsonl.gz")))
            input_files.extend(sorted(p.glob("*.jsonl")))
        elif p.exists():
            input_files.append(p)
        else:
            print(f"WARNING: input not found: {p}", file=sys.stderr)

    if not input_files:
        print("ERROR: no input files found", file=sys.stderr)
        sys.exit(1)

    args.output_dir.mkdir(parents=True, exist_ok=True)

    # In append mode, load existing hashes and shard info
    if args.append:
        global_hashes, existing_shards, existing_rows, processed_files = _load_existing_hashes(args.output_dir)
        start_shard = len(existing_shards)
        # Skip files already processed in previous runs
        before = len(input_files)
        input_files = [f for f in input_files if f.name not in processed_files]
        skipped_files = before - len(input_files)
        print(f"[pack] Append mode: {existing_rows:,} existing rows in {start_shard} shards, "
              f"{len(global_hashes):,} known hashes, "
              f"{skipped_files:,} files already processed (skipped)")
    else:
        global_hashes = set()
        existing_shards = []
        existing_rows = 0
        start_shard = 0
        processed_files = set()

    print(f"[pack] {len(input_files)} input files, {args.workers} workers, "
          f"{args.rows_per_shard:,} rows/shard")

    writer = _ShardWriter(args.output_dir, args.rows_per_shard, start_shard)
    total_kept = 0
    total_skipped = 0
    total_dupes = 0
    started = time.time()

    file_strs = [str(f) for f in input_files]

    if args.workers > 1 and len(input_files) > 1:
        with Pool(args.workers) as pool:
            for i, result in enumerate(pool.imap_unordered(_process_file, file_strs)):
                data_chunks, meta_chunks, hashes, kept, skipped = result
                for j in range(len(data_chunks)):
                    h = struct.unpack("<Q", meta_chunks[j][:8])[0]
                    if h in global_hashes:
                        total_dupes += 1
                        continue
                    global_hashes.add(h)
                    writer.write_row(data_chunks[j], meta_chunks[j])
                    total_kept += 1
                total_skipped += skipped
                if (i + 1) % 1000 == 0 or (i + 1) == len(input_files):
                    elapsed = time.time() - started
                    print(
                        f"[pack] {i+1}/{len(input_files)} files, "
                        f"{total_kept/1e6:.2f}M rows kept, "
                        f"{total_dupes} dupes, "
                        f"{elapsed:.0f}s",
                        flush=True,
                    )
    else:
        for i, file_str in enumerate(file_strs):
            data_chunks, meta_chunks, hashes, kept, skipped = _process_file(file_str)
            for j in range(len(data_chunks)):
                h = struct.unpack("<Q", meta_chunks[j][:8])[0]
                if h in global_hashes:
                    total_dupes += 1
                    continue
                global_hashes.add(h)
                writer.write_row(data_chunks[j], meta_chunks[j])
                total_kept += 1
            total_skipped += skipped
            if (i + 1) % 1000 == 0 or (i + 1) == len(input_files):
                elapsed = time.time() - started
                print(
                    f"[pack] {i+1}/{len(input_files)} files, "
                    f"{total_kept/1e6:.2f}M rows kept, "
                    f"{total_dupes} dupes, "
                    f"{elapsed:.0f}s",
                    flush=True,
                )

    writer.close()
    elapsed = time.time() - started

    # Track all processed files (existing + newly processed)
    processed_files.update(f.name for f in input_files)

    # Build header
    all_shards = existing_shards + writer.shards
    total_rows = existing_rows + total_kept
    header = {
        "format_version": 2,
        "num_rows": total_rows,
        "num_shards": len(all_shards),
        "rows_per_shard": args.rows_per_shard,
        "data_row_bytes": DATA_ROW_BYTES,
        "meta_row_bytes": META_ROW_BYTES,
        "grid_shape": [MAX_GRID_CHANNELS, MAX_GRID_HEIGHT, MAX_GRID_WIDTH],
        "num_scalars": 6,
        "shards": all_shards,
        "processed_files": sorted(processed_files),
        "num_input_files": len(input_files),
        "total_skipped": total_skipped,
        "total_dupes": total_dupes,
        "packing_seconds": round(elapsed, 1),
    }
    header_path = args.output_dir / "header.json"
    header_path.write_text(json.dumps(header, indent=2) + "\n", encoding="utf-8")

    total_data_size = sum(
        (args.output_dir / f"{s['id']}_data.bin").stat().st_size
        for s in all_shards
    )
    total_meta_size = sum(
        (args.output_dir / f"{s['id']}_meta.bin").stat().st_size
        for s in all_shards
    )
    print(f"\n[pack] Done in {elapsed:.0f}s")
    print(f"  Rows: {total_rows:,} ({total_kept:,} new, {total_dupes} dupes)")
    print(f"  Shards: {len(all_shards)} ({len(writer.shards)} new)")
    print(f"  Data: {total_data_size/1e9:.2f} GB")
    print(f"  Meta: {total_meta_size/1e6:.1f} MB")
    print(f"  header.json: {header_path}")


if __name__ == "__main__":
    main()
