#!/usr/bin/env python3
"""Distill03: Full soft targets (all 36.8M) + int4 QAT students (96/64/48ch).

Usage:
    # Step 1: Generate soft targets for ALL positions (~$1, ~30 min A100)
    python -m tools.launch_distill03 --step soft-targets

    # Step 2: Train int4 QAT students in parallel (~$2, ~30 min L40S)
    python -m tools.launch_distill03 --step distill

    # Or run everything sequentially:
    python -m tools.launch_distill03 --step all
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Distill03: full targets + int4 QAT")
    parser.add_argument(
        "--step",
        choices=("soft-targets", "distill", "all"),
        required=True,
    )
    parser.add_argument("--run-id", type=str, default="distill03_full")
    parser.add_argument(
        "--augmented-path",
        type=str,
        default=None,
        help="Override augmented dataset path on Volume",
    )
    parser.add_argument("--dry-run", action="store_true", help="Print specs without launching")
    return parser.parse_args()


SOFT_TARGETS_SPEC = {
    "gpu": "A100",
    "r2_bitpacked_path": "/r2/bitpacked",
    "volume_teacher_model_path": "/data/h100_full_run_v2/teacher/checkpoints/teacher_epoch3.pt",
    "volume_teacher_config_path": "/data/h100_full_run_v2/teacher/teacher_training_config.json",
    "max_samples": 0,  # ALL positions — no subsampling
    "batch_size": 512,
    "num_workers": 4,
    "device_preference": "cuda",
}

# Int4 QAT students for C bot submission
QAT_STUDENTS = [
    {"conv_channels": 96, "num_conv_layers": 3, "distill_temperature": 2.0, "distill_alpha": 0.3, "label": "96ch_int4_T2_a03"},
    {"conv_channels": 64, "num_conv_layers": 3, "distill_temperature": 2.0, "distill_alpha": 0.3, "label": "64ch_int4_T2_a03"},
    {"conv_channels": 48, "num_conv_layers": 3, "distill_temperature": 2.0, "distill_alpha": 0.3, "label": "48ch_int4_T2_a03"},
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
        "qat_bits": 4,
        "epochs": 8,
        "batch_size": 128,
        "learning_rate": 5e-4,
        "weight_decay": 1e-4,
        "optimizer": "adamw",
        "seed": 42,
        "volume_dataset_path": augmented_path,
        "run_id": f"{run_id}/{student['label']}",
    }


def run_soft_targets(args: argparse.Namespace) -> str:
    spec = dict(SOFT_TARGETS_SPEC)
    spec["run_id"] = args.run_id

    print("=== Soft Target Generation (FULL DATASET) ===")
    print(f"  Teacher: 128ch/8-block epoch 3")
    print(f"  Dataset: R2 bitpacked (~36.8M positions)")
    print(f"  Sample:  ALL (max_samples=0)")
    print(f"  GPU:     A100 (~$1-2)")
    print(f"  Spec:    {json.dumps(spec, indent=2)}")
    print()

    if args.dry_run:
        return f"/data/{args.run_id}/augmented"

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


def run_distill(args: argparse.Namespace) -> list[dict]:
    augmented_path = args.augmented_path
    if not augmented_path:
        augmented_path = f"/data/{args.run_id}/augmented"

    specs = [make_distill_spec(s, augmented_path, args.run_id) for s in QAT_STUDENTS]

    print(f"=== Int4 QAT Distillation ({len(QAT_STUDENTS)} jobs) ===")
    for s, spec in zip(QAT_STUDENTS, specs):
        print(f"  {s['label']}: {s['conv_channels']}ch/{s['num_conv_layers']}L T={s['distill_temperature']} α={s['distill_alpha']} qat=int4")
    print(f"  GPU: L40S × {len(QAT_STUDENTS)} parallel")
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
        print(f"[distill] Spawning {len(specs)} parallel int4 QAT jobs...", flush=True)
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

    output_path = REPO_ROOT / f"data/{args.run_id}_qat_results.json"
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

    if args.step in ("distill", "all"):
        run_distill(args)


if __name__ == "__main__":
    main()
