#!/usr/bin/env python3
"""Distill02: Generate soft targets from 128ch teacher, then distill students.

Usage:
    # Step 1: Generate soft targets (~$0.50, ~10 min)
    python -m tools.launch_distill02 --step soft-targets

    # Step 2: Distill all students in parallel (~$2.50, ~15 min)
    python -m tools.launch_distill02 --step distill

    # Or run everything sequentially:
    python -m tools.launch_distill02 --step all
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Distill02 pipeline")
    parser.add_argument(
        "--step",
        choices=("soft-targets", "distill", "distill-rust", "distill-python", "all"),
        required=True,
    )
    parser.add_argument("--run-id", type=str, default="distill02")
    parser.add_argument("--max-samples", type=int, default=2_000_000)
    parser.add_argument(
        "--augmented-path",
        type=str,
        default=None,
        help="Override augmented dataset path on Volume (auto-detected from soft-targets output)",
    )
    parser.add_argument("--dry-run", action="store_true", help="Print specs without launching")
    return parser.parse_args()


SOFT_TARGETS_SPEC = {
    "gpu": "A100",
    "r2_bitpacked_path": "/r2/bitpacked",
    "volume_teacher_model_path": "/data/h100_full_run_v2/teacher/checkpoints/teacher_epoch3.pt",
    "volume_teacher_config_path": "/data/h100_full_run_v2/teacher/teacher_training_config.json",
    "batch_size": 512,
    "device_preference": "cuda",
}

# Rust students: 24ch and 32ch (for search-based hybrid bot)
RUST_STUDENTS = [
    {"conv_channels": 24, "num_conv_layers": 3, "distill_temperature": 2.0, "distill_alpha": 0.3, "label": "24ch_T2_a03"},
    {"conv_channels": 24, "num_conv_layers": 3, "distill_temperature": 3.0, "distill_alpha": 0.5, "label": "24ch_T3_a05"},
    {"conv_channels": 32, "num_conv_layers": 3, "distill_temperature": 2.0, "distill_alpha": 0.3, "label": "32ch_T2_a03"},
    {"conv_channels": 32, "num_conv_layers": 3, "distill_temperature": 3.0, "distill_alpha": 0.5, "label": "32ch_T3_a05"},
    {"conv_channels": 32, "num_conv_layers": 3, "distill_temperature": 2.0, "distill_alpha": 0.5, "label": "32ch_T2_a05"},
    {"conv_channels": 32, "num_conv_layers": 3, "distill_temperature": 5.0, "distill_alpha": 0.3, "label": "32ch_T5_a03"},
]

# Python students: 48ch and 64ch (for pure-NN bot)
PYTHON_STUDENTS = [
    {"conv_channels": 48, "num_conv_layers": 3, "distill_temperature": 2.0, "distill_alpha": 0.3, "label": "48ch_T2_a03"},
    {"conv_channels": 48, "num_conv_layers": 3, "distill_temperature": 3.0, "distill_alpha": 0.5, "label": "48ch_T3_a05"},
    {"conv_channels": 64, "num_conv_layers": 3, "distill_temperature": 2.0, "distill_alpha": 0.3, "label": "64ch_T2_a03"},
    {"conv_channels": 64, "num_conv_layers": 3, "distill_temperature": 3.0, "distill_alpha": 0.5, "label": "64ch_T3_a05"},
]


def make_distill_spec(student: dict, augmented_path: str, run_id: str) -> dict:
    return {
        "gpu": "L40S",
        "device_preference": "cuda",
        "training_mode": "distill",
        "conv_channels": student["conv_channels"],
        "num_conv_layers": student["num_conv_layers"],
        "distill_temperature": student["distill_temperature"],
        "distill_alpha": student["distill_alpha"],
        "epochs": 6,
        "batch_size": 128,
        "learning_rate": 1e-3,
        "weight_decay": 1e-4,
        "optimizer": "adamw",
        "seed": 42,
        "volume_dataset_path": augmented_path,
        "run_id": f"{run_id}/{student['label']}",
    }


def run_soft_targets(args: argparse.Namespace) -> str:
    spec = dict(SOFT_TARGETS_SPEC)
    spec["max_samples"] = args.max_samples
    spec["run_id"] = args.run_id

    print("=== Soft Target Generation ===")
    print(f"  Teacher: 128ch/8-block epoch 3")
    print(f"  Dataset: R2 bitpacked (~36.8M positions)")
    print(f"  Sample:  {args.max_samples:,} positions")
    print(f"  GPU:     A100 (~$0.50)")
    print(f"  Spec:    {json.dumps(spec, indent=2)}")
    print()

    if args.dry_run:
        return f"/data/{args.run_id}/augmented/augmented_dataset.jsonl.gz"

    sys.path.insert(0, str(REPO_ROOT))
    from python.train.outerloop import modal_job

    with modal_job.app.run():
        spec_json = json.dumps(spec)
        print("[soft-targets] Launching on A100...", flush=True)
        start = time.time()
        result = modal_job.generate_soft_targets_a100.remote(spec_json)
        elapsed = time.time() - start

    augmented_path = result.get("volume_augmented_path", "")
    print(f"\n[soft-targets] Done in {elapsed/60:.1f}min")
    print(f"  Rows processed: {result.get('rows_processed', '?'):,}")
    print(f"  Volume path:    {augmented_path}")
    return augmented_path


def run_distill(args: argparse.Namespace, students: list[dict], label: str) -> list[dict]:
    augmented_path = args.augmented_path
    if not augmented_path:
        augmented_path = f"/data/{args.run_id}/augmented"

    specs = [make_distill_spec(s, augmented_path, args.run_id) for s in students]

    print(f"=== Distillation: {label} ({len(students)} jobs) ===")
    for s, spec in zip(students, specs):
        params = s["conv_channels"] * s["conv_channels"] * 9 * s["num_conv_layers"]  # rough
        print(f"  {s['label']}: {s['conv_channels']}ch/{s['num_conv_layers']}L T={s['distill_temperature']} α={s['distill_alpha']}")
    print(f"  GPU: L40S × {len(students)} parallel")
    print(f"  Dataset: {augmented_path}")
    print()

    if args.dry_run:
        for spec in specs:
            print(f"  Spec ({spec['run_id'].split('/')[-1]}):")
            print(f"    {json.dumps(spec, indent=2)}")
        return []

    sys.path.insert(0, str(REPO_ROOT))
    from python.train.outerloop import modal_job

    with modal_job.app.run():
        print(f"[distill] Spawning {len(specs)} parallel jobs...", flush=True)
        start = time.time()
        handles = []
        for spec in specs:
            spec_json = json.dumps(spec)
            handle = modal_job.train_l40s.spawn(spec_json)
            handles.append((spec, handle))

        results = []
        for spec, handle in handles:
            result = handle.get()
            elapsed = time.time() - start
            label_name = spec["run_id"].split("/")[-1]
            metrics = result.get("metrics", {})
            print(
                f"  [{label_name}] val_corr={metrics.get('validation_value_correlation', '?'):.4f} "
                f"pacc={metrics.get('validation_policy_accuracy', '?'):.4f} "
                f"({elapsed/60:.1f}min)",
                flush=True,
            )
            results.append(result)

    # Save results
    output_path = REPO_ROOT / f"data/{args.run_id}_{label}_results.json"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        json.dumps(results, indent=2, sort_keys=True, default=str) + "\n",
        encoding="utf-8",
    )
    print(f"\nResults saved to: {output_path}")
    return results


def main() -> None:
    args = parse_args()

    if args.step in ("soft-targets", "all"):
        augmented_path = run_soft_targets(args)
        if not args.augmented_path:
            args.augmented_path = augmented_path

    if args.step in ("distill", "distill-rust", "all"):
        run_distill(args, RUST_STUDENTS, "rust")

    if args.step in ("distill", "distill-python", "all"):
        run_distill(args, PYTHON_STUDENTS, "python")


if __name__ == "__main__":
    main()
