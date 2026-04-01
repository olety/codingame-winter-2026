#!/usr/bin/env python3
"""Phase B: A/B test Muon vs AdamW on 128ch teacher, parallel on 2× A100 40GB.

Usage:
    python -m tools.launch_muon_ab_test --volume-dataset-path /data/selfplay_ab_2k.jsonl.gz
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Muon vs AdamW A/B test on A100")
    parser.add_argument("--dataset-path", type=str, default=None, help="Local or repo-relative dataset path")
    parser.add_argument("--volume-dataset-path", type=str, default=None, help="Modal Volume dataset path")
    parser.add_argument("--epochs", type=int, default=20)
    parser.add_argument("--batch-size", type=int, default=256)
    parser.add_argument("--max-samples", type=int, default=0, help="0 = use all")
    parser.add_argument("--teacher-channels", type=int, default=128)
    parser.add_argument("--teacher-blocks", type=int, default=8)
    parser.add_argument("--run-id", type=str, default=None)
    return parser.parse_args()


def make_spec(
    *,
    optimizer: str,
    args: argparse.Namespace,
    run_id: str,
) -> dict:
    spec: dict = {
        "gpu": "A100",
        "device_preference": "cuda",
        "teacher_conv_channels": args.teacher_channels,
        "teacher_num_res_blocks": args.teacher_blocks,
        "epochs": args.epochs,
        "batch_size": args.batch_size,
        "max_samples": args.max_samples,
        "optimizer": optimizer,
        "run_id": f"{run_id}_{optimizer}",
        "seed": 42,
    }
    if optimizer == "muon":
        spec["learning_rate"] = 0.02
        spec["muon_momentum"] = 0.95
        spec["adamw_lr"] = 3e-4
        spec["weight_decay"] = 1e-4
    else:
        spec["learning_rate"] = 3e-4
        spec["weight_decay"] = 1e-4

    if args.volume_dataset_path:
        spec["volume_dataset_path"] = args.volume_dataset_path
    elif args.dataset_path:
        spec["dataset_path"] = args.dataset_path
    else:
        print("ERROR: Must provide --dataset-path or --volume-dataset-path", file=sys.stderr)
        sys.exit(1)
    return spec


def main() -> None:
    args = parse_args()
    run_id = args.run_id or f"muon_ab_{int(time.time())}"

    adamw_spec = make_spec(optimizer="adamw", args=args, run_id=run_id)
    muon_spec = make_spec(optimizer="muon", args=args, run_id=run_id)

    print(f"=== Muon vs AdamW A/B Test ===")
    print(f"Run ID: {run_id}")
    print(f"Teacher: {args.teacher_channels}ch / {args.teacher_blocks} blocks")
    print(f"Epochs: {args.epochs}, Batch size: {args.batch_size}")
    print(f"GPU: A100 40GB × 2 (parallel)")
    print()

    sys.path.insert(0, str(REPO_ROOT))
    from python.train.outerloop import modal_job

    # Single app.run() context — spawn both jobs in parallel
    with modal_job.app.run():
        adamw_json = json.dumps(adamw_spec)
        muon_json = json.dumps(muon_spec)

        print("[adamw] Spawning teacher training on A100...")
        print(f"[adamw] Spec: optimizer=adamw, lr={adamw_spec['learning_rate']}, epochs={adamw_spec['epochs']}")
        print("[muon] Spawning teacher training on A100...")
        print(f"[muon] Spec: optimizer=muon, lr={muon_spec['learning_rate']}, epochs={muon_spec['epochs']}")

        start = time.time()
        adamw_handle = modal_job.train_teacher_a100.spawn(adamw_json)
        muon_handle = modal_job.train_teacher_a100.spawn(muon_json)

        # Wait for both
        adamw_result = adamw_handle.get()
        adamw_elapsed = time.time() - start
        print(f"[adamw] Done in {adamw_elapsed/60:.1f}min")

        muon_result = muon_handle.get()
        muon_elapsed = time.time() - start
        print(f"[muon] Done in {muon_elapsed/60:.1f}min")

    results = {}
    for label, result, elapsed in [("adamw", adamw_result, adamw_elapsed), ("muon", muon_result, muon_elapsed)]:
        result["wallclock_seconds"] = elapsed
        results[label] = result

    print()
    print("=== A/B Test Results ===")
    for label in ("adamw", "muon"):
        r = results[label]
        m = r.get("metrics", {})
        if not m:
            print(f"  {label}: No metrics returned — check logs")
            continue
        print(f"  {label}:")
        print(f"    val_mae:        {m.get('validation_value_mae', '?'):.4f}")
        print(f"    val_corr:       {m.get('validation_value_correlation', '?'):.4f}")
        print(f"    val_policy_acc: {m.get('validation_policy_accuracy', '?'):.4f}")
        print(f"    wallclock:      {elapsed/60:.1f}min")
        if m.get("volume_model_path"):
            print(f"    model_path:     {m['volume_model_path']}")

    # Pick winner
    adamw_m = results.get("adamw", {}).get("metrics", {})
    muon_m = results.get("muon", {}).get("metrics", {})
    if adamw_m and muon_m:
        adamw_score = adamw_m.get("validation_value_correlation", 0)
        muon_score = muon_m.get("validation_value_correlation", 0)
        winner = "muon" if muon_score > adamw_score else "adamw"
        print(f"\n  Winner: {winner} (corr {max(adamw_score, muon_score):.4f} vs {min(adamw_score, muon_score):.4f})")

    # Save results
    output_path = REPO_ROOT / f"data/ab_test_{run_id}.json"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(results, indent=2, sort_keys=True, default=str) + "\n", encoding="utf-8")
    print(f"\nFull results saved to: {output_path}")


if __name__ == "__main__":
    main()
