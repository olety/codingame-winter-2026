#!/usr/bin/env python3
"""Phase C: Train 512ch/16-block SE teacher on full self-play dataset.

Usage:
    # Upload data to Modal Volume first:
    #   modal volume put snakebot-datasets selfplay_merged.jsonl.gz /selfplay_merged.jsonl.gz
    #
    # Then run:
    python -m tools.launch_teacher_training \
        --volume-dataset-path /data/selfplay_merged.jsonl.gz \
        --teacher-channels 512 --teacher-blocks 16 \
        --optimizer muon --epochs 30
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Train large teacher model on A100")
    parser.add_argument("--dataset-path", type=str, default=None, help="Local or repo-relative dataset path")
    parser.add_argument("--volume-dataset-path", type=str, default=None, help="Modal Volume dataset path")
    parser.add_argument("--optimizer", type=str, default="adamw", choices=["adamw", "muon"])
    parser.add_argument("--epochs", type=int, default=30)
    parser.add_argument("--batch-size", type=int, default=256)
    parser.add_argument("--max-samples", type=int, default=0)
    parser.add_argument("--teacher-channels", type=int, default=512)
    parser.add_argument("--teacher-blocks", type=int, default=16)
    parser.add_argument("--learning-rate", type=float, default=None, help="Override LR (default: 3e-4 for adamw, 0.02 for muon)")
    parser.add_argument("--run-id", type=str, default=None)
    parser.add_argument("--gpu", type=str, default="A100")
    parser.add_argument("--resume-checkpoint", type=str, default=None, help="Volume path to checkpoint .pt to resume from")
    parser.add_argument("--r2-bitpacked-path", type=str, default=None, help="R2-mounted bitpacked dataset path (e.g. /r2/bitpacked)")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    run_id = args.run_id or f"teacher_{args.teacher_channels}ch_{int(time.time())}"

    lr = args.learning_rate
    if lr is None:
        lr = 0.02 if args.optimizer == "muon" else 3e-4

    spec: dict = {
        "gpu": args.gpu,
        "device_preference": "cuda",
        "teacher_conv_channels": args.teacher_channels,
        "teacher_num_res_blocks": args.teacher_blocks,
        "epochs": args.epochs,
        "batch_size": args.batch_size,
        "max_samples": args.max_samples,
        "optimizer": args.optimizer,
        "learning_rate": lr,
        "weight_decay": 1e-4,
        "run_id": run_id,
        "seed": 42,
    }
    if args.optimizer == "muon":
        spec["muon_momentum"] = 0.95
        spec["adamw_lr"] = 3e-4

    if args.r2_bitpacked_path:
        spec["r2_bitpacked_path"] = args.r2_bitpacked_path
    elif args.volume_dataset_path:
        spec["volume_dataset_path"] = args.volume_dataset_path
    elif args.dataset_path:
        spec["dataset_path"] = args.dataset_path
    else:
        print("ERROR: Must provide --dataset-path, --volume-dataset-path, or --r2-bitpacked-path", file=sys.stderr)
        sys.exit(1)

    if args.resume_checkpoint:
        spec["resume_checkpoint"] = args.resume_checkpoint

    print(f"=== Teacher Training ===")
    print(f"Run ID: {run_id}")
    print(f"Architecture: {args.teacher_channels}ch / {args.teacher_blocks} SE-res blocks")
    print(f"Optimizer: {args.optimizer} (lr={lr})")
    print(f"Epochs: {args.epochs}, Batch size: {args.batch_size}")
    print(f"GPU: {args.gpu}")
    print()

    sys.path.insert(0, str(REPO_ROOT))
    from python.train.outerloop.launch_modal import launch_modal

    start = time.time()
    result = launch_modal("train-teacher", spec)
    elapsed = time.time() - start

    metrics = result.get("metrics", {})
    print(f"\n=== Training Complete ({elapsed/60:.1f}min) ===")
    print(f"  val_mae:        {metrics.get('validation_value_mae', '?'):.4f}")
    print(f"  val_corr:       {metrics.get('validation_value_correlation', '?'):.4f}")
    print(f"  val_policy_acc: {metrics.get('validation_policy_accuracy', '?'):.4f}")
    print(f"  train_v_loss:   {metrics.get('train_value_loss', '?'):.4f}")
    print(f"  train_p_loss:   {metrics.get('train_policy_loss', '?'):.4f}")

    if metrics.get("volume_model_path"):
        print(f"\n  Teacher model on Volume: {metrics['volume_model_path']}")
        print(f"  Teacher config on Volume: {metrics['volume_config_path']}")
        print(f"\n  Next step — generate soft targets:")
        print(f"    python -m tools.launch_teacher_training --generate-soft-targets \\")
        print(f"        --volume-teacher-model {metrics['volume_model_path']} \\")
        print(f"        --volume-teacher-config {metrics['volume_config_path']}")

    # Save results
    output_path = REPO_ROOT / f"data/teacher_{run_id}.json"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(result, indent=2, sort_keys=True, default=str) + "\n", encoding="utf-8")
    print(f"\nFull results saved to: {output_path}")


if __name__ == "__main__":
    main()
