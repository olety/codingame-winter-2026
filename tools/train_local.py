#!/usr/bin/env python3
"""Local QAT distillation training with chunk cycling.

When --max-samples is set, cycles through chunks of the full dataset:
  chunk 0: rows 0, 3, 6, 9, ...
  chunk 1: rows 1, 4, 7, 10, ...
  chunk 2: rows 2, 5, 8, 11, ...
Each chunk is preloaded into RAM, trained for --epochs-per-chunk epochs,
then swapped for the next chunk. Repeats --num-cycles times.

Usage:
    # Full dataset via mmap (slow for small models):
    python3 -m tools.train_local --channels 16 --max-samples 0

    # Chunk cycling (fast, uses all data):
    python3 -m tools.train_local --channels 16 --max-samples 10000000 --epochs-per-chunk 3 --num-cycles 4
"""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from python.train.outerloop.train_model import train_distill_from_spec
from python.train.outerloop.export_weights import export_weights_int4


def main():
    parser = argparse.ArgumentParser(description="Local QAT distillation training")
    parser.add_argument("--channels", type=int, required=True)
    parser.add_argument("--layers", type=int, default=3)
    parser.add_argument("--epochs", type=int, default=10,
                        help="Total epochs (used when no chunk cycling)")
    parser.add_argument("--epochs-per-chunk", type=int, default=3,
                        help="Epochs to train per chunk before swapping data")
    parser.add_argument("--num-cycles", type=int, default=0,
                        help="Number of full cycles through all chunks (0=auto based on --epochs)")
    parser.add_argument("--batch-size", type=int, default=512)
    parser.add_argument("--lr", type=float, default=5e-4)
    parser.add_argument("--device", type=str, default="mps", choices=["mps", "cuda", "cpu"])
    parser.add_argument("--predictions-path", type=str, default="data/distill03_predictions")
    parser.add_argument("--bitpacked-path", type=str, default="data/bitpacked")
    parser.add_argument("--output-dir", type=str, default=None)
    parser.add_argument("--resume", type=str, default=None)
    parser.add_argument("--temperature", type=float, default=2.0)
    parser.add_argument("--alpha", type=float, default=0.3)
    parser.add_argument("--max-samples", type=int, default=10000000,
                        help="Rows per chunk (0=all rows, no chunking)")
    parser.add_argument("--early-stop-patience", type=int, default=3)
    parser.add_argument("--num-workers", type=int, default=0)
    args = parser.parse_args()

    output_dir = args.output_dir or f"data/distill03_local/{args.channels}ch_int4"
    Path(output_dir).mkdir(parents=True, exist_ok=True)

    if args.max_samples == 0:
        # No chunking — full dataset via mmap
        spec = {
            "dataset_path": args.predictions_path,
            "bitpacked_base_path": args.bitpacked_path,
            "output_dir": output_dir,
            "device_preference": args.device,
            "conv_channels": args.channels,
            "num_conv_layers": args.layers,
            "qat_bits": 4,
            "epochs": args.epochs,
            "batch_size": args.batch_size,
            "learning_rate": args.lr,
            "weight_decay": 1e-4,
            "distill_temperature": args.temperature,
            "distill_alpha": args.alpha,
            "preload_dataset": False,
            "max_samples": 0,
            "early_stop_patience": args.early_stop_patience,
            "num_workers": args.num_workers,
            "seed": 42,
        }
        if args.resume:
            spec["resume_checkpoint"] = args.resume

        print(f"=== {args.channels}ch/{args.layers}L int4 — full dataset mmap ===", flush=True)
        metrics = train_distill_from_spec(spec)
    else:
        # Chunk cycling — preload max_samples, train, swap chunk, repeat
        # Figure out how many chunks exist
        n_chunks = 36_772_411 // args.max_samples  # approximate
        if n_chunks < 1:
            n_chunks = 1

        if args.num_cycles > 0:
            total_cycles = args.num_cycles
        else:
            # Auto: enough cycles so total epochs ≈ args.epochs
            total_epochs_target = args.epochs
            total_cycles = max(1, total_epochs_target // args.epochs_per_chunk)

        resume_path = args.resume
        total_epoch = 0
        best_pacc = 0.0
        stale_cycles = 0

        print(f"=== {args.channels}ch/{args.layers}L int4 — chunk cycling ===", flush=True)
        print(f"  {args.max_samples/1e6:.0f}M rows/chunk, {n_chunks} chunks, "
              f"{args.epochs_per_chunk} epochs/chunk, {total_cycles} cycles", flush=True)
        print(f"  Total epochs: ~{total_cycles * args.epochs_per_chunk * n_chunks}", flush=True)
        print(flush=True)

        metrics = None
        for cycle in range(total_cycles):
            for chunk in range(n_chunks):
                spec = {
                    "dataset_path": args.predictions_path,
                    "bitpacked_base_path": args.bitpacked_path,
                    "output_dir": output_dir,
                    "device_preference": args.device,
                    "conv_channels": args.channels,
                    "num_conv_layers": args.layers,
                    "qat_bits": 4,
                    "epochs": args.epochs_per_chunk,
                    "batch_size": args.batch_size,
                    "learning_rate": args.lr,
                    "weight_decay": 1e-4,
                    "distill_temperature": args.temperature,
                    "distill_alpha": args.alpha,
                    "preload_dataset": True,
                    "max_samples": args.max_samples,
                    "chunk_offset": chunk,
                    "early_stop_patience": 0,  # don't early-stop within a chunk
                    "num_workers": args.num_workers,
                    "seed": 42,
                }
                if resume_path:
                    spec["resume_checkpoint"] = resume_path
                    # Override epoch range: resume weights only, reset epoch counter
                    # so we train epochs_per_chunk MORE epochs, not up-to
                    import torch as _torch
                    _ckpt = _torch.load(resume_path, map_location="cpu", weights_only=False)
                    _prev_epoch = _ckpt.get("epoch", 0)
                    spec["epochs"] = _prev_epoch + args.epochs_per_chunk

                print(f"\n--- Cycle {cycle+1}/{total_cycles}, "
                      f"Chunk {chunk+1}/{n_chunks} ---", flush=True)
                metrics = train_distill_from_spec(spec)
                total_epoch += args.epochs_per_chunk

                # Track best and set up resume
                ckpt_dir = Path(output_dir) / "checkpoints"
                latest_ckpt = sorted(ckpt_dir.glob("distill_epoch*.pt"))
                if latest_ckpt:
                    resume_path = str(latest_ckpt[-1])

                pacc = metrics.get("validation_policy_accuracy", 0)
                if pacc > best_pacc:
                    best_pacc = pacc
                    stale_cycles = 0
                else:
                    stale_cycles += 1

            # Early stop across cycles
            if args.early_stop_patience > 0 and stale_cycles >= args.early_stop_patience * n_chunks:
                print(f"\n[chunk-train] Early stopping after cycle {cycle+1} "
                      f"(no improvement in {args.early_stop_patience} cycles, best pacc={best_pacc:.4f})",
                      flush=True)
                break

    print(f"\n=== Training Complete ===", flush=True)
    if metrics:
        print(json.dumps(metrics, indent=2, sort_keys=True), flush=True)

    # Export int4 weights
    model_path = str(Path(output_dir) / "hybrid_model.pt")
    config_path = str(Path(output_dir) / "training_config.json")
    weights_path = str(Path(output_dir) / f"weights_int4_{args.channels}ch.json")
    if Path(model_path).exists():
        print(f"\nExporting int4 weights to {weights_path}...", flush=True)
        export_weights_int4(Path(model_path), Path(config_path), Path(weights_path))
        print(f"Done.", flush=True)


if __name__ == "__main__":
    main()
