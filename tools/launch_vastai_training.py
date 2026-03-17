#!/usr/bin/env python3
"""Launch teacher training on vast.ai with bitpacked data from R2.

Prerequisites:
    pip install vastai
    vastai set api-key <YOUR_API_KEY>

    # R2 credentials as env vars (or in ~/.aws/credentials):
    export R2_ACCESS_KEY_ID=...
    export R2_SECRET_ACCESS_KEY=...

Usage:
    python3 -m tools.launch_vastai_training \
        --gpu A100_SXM4 --max-price 1.00 \
        --r2-bitpacked-path bitpacked \
        --resume-checkpoint teacher_epoch1.pt \
        --teacher-channels 512 --teacher-blocks 16 \
        --optimizer muon --epochs 20 --batch-size 512
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import textwrap
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
R2_ENDPOINT = "https://70ae12731f3e503777d9f59a6c2c18da.r2.cloudflarestorage.com"
R2_BUCKET = "snakebot-data"


def run(cmd: str | list[str], **kwargs) -> subprocess.CompletedProcess:
    if isinstance(cmd, str):
        cmd = cmd.split()
    result = subprocess.run(cmd, capture_output=True, text=True, **kwargs)
    if result.returncode != 0:
        print(f"FAILED: {' '.join(cmd)}", file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        sys.exit(1)
    return result


def search_offers(gpu_name: str, max_price: float, min_vram: int = 40, min_disk: int = 100) -> dict | None:
    """Search for cheapest matching vast.ai offer."""
    query = (
        f"gpu_name={gpu_name} "
        f"gpu_ram>={min_vram} "
        f"num_gpus=1 "
        f"dph<={max_price} "
        f"inet_down>=200 "
        f"reliability>0.95 "
        f"rented=False "
        f"datacenter=True"
    )
    result = subprocess.run(
        ["vastai", "search", "offers", query, "--raw", "-o", "dph"],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        print(f"vastai search failed: {result.stderr}", file=sys.stderr)
        return None

    try:
        offers = json.loads(result.stdout)
    except json.JSONDecodeError:
        print(f"Failed to parse offers: {result.stdout[:200]}", file=sys.stderr)
        return None

    if not offers:
        return None
    return offers[0]


def create_instance(offer_id: int, disk_gb: int, onstart_script: str) -> int:
    """Create a vast.ai SSH instance and return instance ID."""
    result = run([
        "vastai", "create", "instance", str(offer_id),
        "--image", "pytorch/pytorch:2.10.0-cuda12.6-cudnn9-devel",
        "--disk", str(disk_gb),
        "--ssh",
        "--direct",
        "--onstart-cmd", onstart_script,
    ])
    # Output is like "new instance ID: 12345\n" or JSON
    output = result.stdout.strip()
    try:
        data = json.loads(output)
        return int(data.get("new_contract", data.get("id", 0)))
    except (json.JSONDecodeError, ValueError):
        # Parse "new instance ID: 12345"
        for word in output.split():
            if word.isdigit():
                return int(word)
    print(f"Could not parse instance ID from: {output}", file=sys.stderr)
    sys.exit(1)


def wait_for_instance(instance_id: int, timeout: int = 300) -> dict:
    """Wait for instance to be running and SSH-ready."""
    print(f"Waiting for instance {instance_id} to start...", flush=True)
    start = time.time()
    while time.time() - start < timeout:
        result = subprocess.run(
            ["vastai", "show", "instances", "--raw"],
            capture_output=True, text=True,
        )
        if result.returncode == 0:
            instances = json.loads(result.stdout)
            for inst in instances:
                if inst.get("id") == instance_id and inst.get("actual_status") == "running":
                    print(f"Instance {instance_id} is running!", flush=True)
                    return inst
        time.sleep(10)
    print(f"Timeout waiting for instance {instance_id}", file=sys.stderr)
    sys.exit(1)


def get_ssh_cmd(instance_id: int) -> str:
    """Get SSH command for instance."""
    result = run(["vastai", "ssh-url", str(instance_id)])
    return result.stdout.strip()


def build_training_script(args: argparse.Namespace, run_id: str) -> str:
    """Build the shell script that runs on the vast.ai instance."""
    r2_access_key = os.environ.get("R2_ACCESS_KEY_ID", "")
    r2_secret_key = os.environ.get("R2_SECRET_ACCESS_KEY", "")

    if not r2_access_key or not r2_secret_key:
        print("ERROR: R2_ACCESS_KEY_ID and R2_SECRET_ACCESS_KEY must be set", file=sys.stderr)
        sys.exit(1)

    lr = args.learning_rate
    if lr is None:
        lr = 0.02 if args.optimizer == "muon" else 3e-4

    resume_flag = ""
    if args.resume_checkpoint:
        resume_flag = f"""
# Download checkpoint from R2
echo "[vastai] Downloading checkpoint..."
aws s3 cp s3://{R2_BUCKET}/checkpoints/{args.resume_checkpoint} /workspace/checkpoint.pt \\
    --endpoint-url {R2_ENDPOINT}
RESUME_CKPT="--resume-checkpoint /workspace/checkpoint.pt"
"""

    return textwrap.dedent(f"""\
        #!/bin/bash
        set -euo pipefail
        exec > >(tee /workspace/training.log) 2>&1

        echo "[vastai] Setting up environment..."

        # Install dependencies
        pip install -q numpy "git+https://github.com/KellerJordan/Muon" awscli

        # Configure AWS for R2
        aws configure set aws_access_key_id {r2_access_key}
        aws configure set aws_secret_access_key {r2_secret_key}
        aws configure set default.region auto

        # Download bitpacked data from R2
        echo "[vastai] Downloading bitpacked dataset from R2..."
        mkdir -p /workspace/bitpacked
        aws s3 cp s3://{R2_BUCKET}/{args.r2_bitpacked_path}/ /workspace/bitpacked/ --recursive \\
            --endpoint-url {R2_ENDPOINT}
        echo "[vastai] Download complete:"
        ls -lh /workspace/bitpacked/

        {resume_flag}

        # Clone repo (lightweight — data excluded)
        echo "[vastai] Cloning training code..."
        pip install -q gitpython
        git clone --depth 1 https://github.com/oneiron-dev/codingame-winter-2026.git /workspace/repo
        cd /workspace/repo
        export PYTHONPATH=/workspace/repo

        # Build training spec
        cat > /workspace/train_spec.json << 'SPECEOF'
        {{
            "dataset_path": "/workspace/bitpacked",
            "output_dir": "/workspace/output",
            "device_preference": "cuda",
            "teacher_conv_channels": {args.teacher_channels},
            "teacher_num_res_blocks": {args.teacher_blocks},
            "epochs": {args.epochs},
            "batch_size": {args.batch_size},
            "optimizer": "{args.optimizer}",
            "learning_rate": {lr},
            "weight_decay": 0.0001,
            "run_id": "{run_id}",
            "seed": 42,
            "num_workers": 4,
            "checkpoint_interval": 1{f',{chr(10)}            "resume_checkpoint": "/workspace/checkpoint.pt"' if args.resume_checkpoint else ''}
            {f',{chr(10)}            "muon_momentum": 0.95, "adamw_lr": 0.0003' if args.optimizer == 'muon' else ''}
        }}
        SPECEOF

        echo "[vastai] Training spec:"
        cat /workspace/train_spec.json

        # Run training
        echo "[vastai] Starting training..."
        python3 -c "
        import json, sys
        sys.path.insert(0, '/workspace/repo')
        from python.train.outerloop.train_model import train_teacher_from_spec
        spec = json.load(open('/workspace/train_spec.json'))
        metrics = train_teacher_from_spec(spec)
        json.dump(metrics, open('/workspace/metrics.json', 'w'), indent=2)
        print(json.dumps(metrics, indent=2))
        "

        # Upload checkpoints and results to R2
        echo "[vastai] Uploading results to R2..."
        aws s3 cp /workspace/output/ s3://{R2_BUCKET}/training_runs/{run_id}/ --recursive \\
            --endpoint-url {R2_ENDPOINT}
        aws s3 cp /workspace/metrics.json s3://{R2_BUCKET}/training_runs/{run_id}/metrics.json \\
            --endpoint-url {R2_ENDPOINT}
        aws s3 cp /workspace/training.log s3://{R2_BUCKET}/training_runs/{run_id}/training.log \\
            --endpoint-url {R2_ENDPOINT}

        echo "[vastai] Training complete! Results uploaded to s3://{R2_BUCKET}/training_runs/{run_id}/"
    """)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Launch teacher training on vast.ai")
    parser.add_argument("--gpu", type=str, default="A100_SXM4",
                        help="GPU name filter (e.g. A100_SXM4, A100_PCIE, H100_SXM5, H100_PCIE)")
    parser.add_argument("--max-price", type=float, default=1.00, help="Max $/hr")
    parser.add_argument("--min-vram", type=int, default=40, help="Min VRAM in GB")
    parser.add_argument("--disk", type=int, default=120, help="Disk size in GB")
    parser.add_argument("--r2-bitpacked-path", type=str, default="bitpacked",
                        help="Path within R2 bucket to bitpacked data")
    parser.add_argument("--resume-checkpoint", type=str, default=None,
                        help="Checkpoint filename in R2 checkpoints/ dir")
    parser.add_argument("--optimizer", type=str, default="muon", choices=["adamw", "muon"])
    parser.add_argument("--epochs", type=int, default=20)
    parser.add_argument("--batch-size", type=int, default=512)
    parser.add_argument("--teacher-channels", type=int, default=512)
    parser.add_argument("--teacher-blocks", type=int, default=16)
    parser.add_argument("--learning-rate", type=float, default=None)
    parser.add_argument("--run-id", type=str, default=None)
    parser.add_argument("--dry-run", action="store_true", help="Print script without launching")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    run_id = args.run_id or f"teacher_{args.teacher_channels}ch_{int(time.time())}"

    print(f"=== Vast.ai Teacher Training ===")
    print(f"Run ID: {run_id}")
    print(f"GPU: {args.gpu} (max ${args.max_price}/hr)")
    print(f"Architecture: {args.teacher_channels}ch / {args.teacher_blocks} blocks")
    print(f"Optimizer: {args.optimizer}")
    print(f"Epochs: {args.epochs}, Batch size: {args.batch_size}")
    print()

    script = build_training_script(args, run_id)

    if args.dry_run:
        print("=== Training Script (dry run) ===")
        print(script)
        return

    # Search for offers
    print(f"Searching for {args.gpu} offers (max ${args.max_price}/hr)...", flush=True)
    offer = search_offers(args.gpu, args.max_price, args.min_vram)
    if not offer:
        # Try broader search without datacenter restriction
        print("No datacenter offers found, trying community hosts...", flush=True)
        offer = search_offers(args.gpu, args.max_price, args.min_vram)

    if not offer:
        print(f"ERROR: No {args.gpu} offers found under ${args.max_price}/hr", file=sys.stderr)
        print("Try increasing --max-price or using a different --gpu", file=sys.stderr)
        sys.exit(1)

    offer_id = offer["id"]
    price = offer.get("dph_total", offer.get("dph", "?"))
    gpu_name = offer.get("gpu_name", args.gpu)
    vram = offer.get("gpu_ram", "?")
    inet = offer.get("inet_down", "?")

    print(f"Found offer #{offer_id}: {gpu_name} {vram}GB, ${price}/hr, {inet} Mbps down")
    print(f"Estimated cost: ${float(price) * args.epochs * 35 / 60:.0f} for {args.epochs} epochs")
    print()

    # Write onstart script to temp file
    script_path = Path(f"/tmp/vastai_train_{run_id}.sh")
    script_path.write_text(script)

    # Create instance
    print(f"Creating instance on offer #{offer_id}...", flush=True)
    instance_id = create_instance(offer_id, args.disk, script)

    # Wait for it to be running
    inst = wait_for_instance(instance_id)

    ssh_url = get_ssh_cmd(instance_id)
    print(f"\n=== Instance Running ===")
    print(f"Instance ID: {instance_id}")
    print(f"SSH: {ssh_url}")
    print(f"Logs: vastai logs {instance_id} --tail 100")
    print(f"Stop: vastai destroy instance {instance_id}")
    print(f"\nResults will be uploaded to: s3://{R2_BUCKET}/training_runs/{run_id}/")
    print(f"\nMonitor with:")
    print(f"  vastai logs {instance_id} --tail 50")
    print(f"  # Or SSH in and: tail -f /workspace/training.log")


if __name__ == "__main__":
    main()
