#!/usr/bin/env python3
"""Parallel hyperparameter sweep on vast.ai for teacher training.

Launches N vast.ai instances simultaneously, each running 2 epochs with
different hyperparams on the bitpacked dataset from R2. Results uploaded
to R2 for comparison.

Prerequisites:
    pip install vastai
    vastai set api-key <KEY>
    export R2_ACCESS_KEY_ID=... R2_SECRET_ACCESS_KEY=...

Usage:
    # Dry run — show configs and estimated cost:
    python3 -m tools.sweep_vastai --dry-run

    # Launch sweep:
    R2_ACCESS_KEY_ID=$(skate get snakebot-r2-accesskey-id) \
    R2_SECRET_ACCESS_KEY=$(skate get snakebot-r2-secret) \
    python3 -m tools.sweep_vastai

    # Check results:
    python3 -m tools.sweep_vastai --results <sweep_id>
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import textwrap
import time

R2_ENDPOINT = "https://70ae12731f3e503777d9f59a6c2c18da.r2.cloudflarestorage.com"
R2_BUCKET = "snakebot-data"

# ── Sweep grid ──────────────────────────────────────────────────────────
SWEEP_CONFIGS = [
    # LR sweep (most important for Muon)
    {"name": "lr0.005",  "learning_rate": 0.005, "batch_size": 512, "weight_decay": 1e-4, "policy_loss_weight": 1.0},
    {"name": "lr0.01",   "learning_rate": 0.01,  "batch_size": 512, "weight_decay": 1e-4, "policy_loss_weight": 1.0},
    {"name": "lr0.02",   "learning_rate": 0.02,  "batch_size": 512, "weight_decay": 1e-4, "policy_loss_weight": 1.0},  # baseline
    {"name": "lr0.04",   "learning_rate": 0.04,  "batch_size": 512, "weight_decay": 1e-4, "policy_loss_weight": 1.0},
    # Batch size sweep
    {"name": "bs1024",   "learning_rate": 0.02,  "batch_size": 1024, "weight_decay": 1e-4, "policy_loss_weight": 1.0},
    # Regularization sweep
    {"name": "wd1e-3",   "learning_rate": 0.02,  "batch_size": 512, "weight_decay": 1e-3, "policy_loss_weight": 1.0},
]


def _build_onstart(config: dict, sweep_id: str, r2_access: str, r2_secret: str) -> str:
    """Build the shell script for one sweep trial."""
    name = config["name"]
    muon_args = ""
    if True:  # always Muon for teacher
        muon_args = f'"muon_momentum": 0.95, "adamw_lr": 0.0003,'

    return textwrap.dedent(f"""\
        #!/bin/bash
        set -euo pipefail
        exec > >(tee /workspace/training.log) 2>&1

        echo "[sweep:{name}] Setting up..."
        pip install -q numpy "git+https://github.com/KellerJordan/Muon" awscli

        aws configure set aws_access_key_id {r2_access}
        aws configure set aws_secret_access_key {r2_secret}
        aws configure set default.region auto

        echo "[sweep:{name}] Downloading bitpacked dataset..."
        mkdir -p /workspace/bitpacked
        aws s3 sync s3://{R2_BUCKET}/bitpacked/ /workspace/bitpacked/ \\
            --endpoint-url {R2_ENDPOINT}
        echo "[sweep:{name}] Download complete: $(cat /workspace/bitpacked/header.json | python3 -c 'import json,sys;print(json.load(sys.stdin)["num_rows"])') rows"

        echo "[sweep:{name}] Cloning training code..."
        git clone --depth 1 https://github.com/oneiron-dev/codingame-winter-2026.git /workspace/repo

        cat > /workspace/train_spec.json << 'SPECEOF'
        {{
            "dataset_path": "/workspace/bitpacked",
            "output_dir": "/workspace/output",
            "device_preference": "cuda",
            "teacher_conv_channels": 512,
            "teacher_num_res_blocks": 16,
            "epochs": 2,
            "batch_size": {config['batch_size']},
            "optimizer": "muon",
            "learning_rate": {config['learning_rate']},
            "weight_decay": {config['weight_decay']},
            {muon_args}
            "num_workers": 4,
            "checkpoint_interval": 1,
            "run_id": "{sweep_id}_{name}",
            "seed": 42
        }}
        SPECEOF

        echo "[sweep:{name}] Config:"
        cat /workspace/train_spec.json
        echo "[sweep:{name}] Starting training (2 epochs)..."

        cd /workspace/repo
        export PYTHONPATH=/workspace/repo
        python3 -c "
        import json, sys, time
        sys.path.insert(0, '/workspace/repo')
        from python.train.outerloop.train_model import train_teacher_from_spec
        spec = json.load(open('/workspace/train_spec.json'))
        metrics = train_teacher_from_spec(spec)
        # Add config info to metrics
        metrics['sweep_config'] = '{name}'
        metrics['sweep_lr'] = {config['learning_rate']}
        metrics['sweep_bs'] = {config['batch_size']}
        metrics['sweep_wd'] = {config['weight_decay']}
        json.dump(metrics, open('/workspace/metrics.json', 'w'), indent=2)
        print(json.dumps(metrics, indent=2))
        "

        echo "[sweep:{name}] Uploading results..."
        aws s3 cp /workspace/metrics.json \\
            s3://{R2_BUCKET}/sweeps/{sweep_id}/{name}/metrics.json \\
            --endpoint-url {R2_ENDPOINT}
        aws s3 cp /workspace/training.log \\
            s3://{R2_BUCKET}/sweeps/{sweep_id}/{name}/training.log \\
            --endpoint-url {R2_ENDPOINT}

        echo "[sweep:{name}] DONE"
    """)


def search_offers(gpu_name: str, max_price: float, count: int) -> list[dict]:
    """Search for N available offers."""
    query = (
        f"gpu_name={gpu_name} "
        f"num_gpus=1 "
        f"dph<={max_price} "
        f"disk_space>=100 "
        f"inet_down>=200 "
        f"reliability>0.95 "
        f"rented=False"
    )
    result = subprocess.run(
        ["vastai", "search", "offers", query, "--raw", "-o", "dph"],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        print(f"vastai search failed: {result.stderr}", file=sys.stderr)
        return []
    try:
        offers = json.loads(result.stdout)
    except json.JSONDecodeError:
        return []
    return offers[:count]


def upload_script_to_r2(script: str, sweep_id: str, config_name: str, r2_access: str, r2_secret: str) -> str:
    """Upload training script to R2, return the S3 path."""
    import tempfile
    s3_path = f"sweeps/{sweep_id}/{config_name}/run.sh"
    with tempfile.NamedTemporaryFile(mode="w", suffix=".sh", delete=False) as f:
        f.write(script)
        tmp_path = f.name
    subprocess.run(
        ["aws", "s3", "cp", tmp_path, f"s3://{R2_BUCKET}/{s3_path}",
         "--endpoint-url", R2_ENDPOINT],
        capture_output=True, text=True,
    )
    os.unlink(tmp_path)
    return s3_path


def create_instance(offer_id: int, onstart: str, disk: int = 120) -> int | None:
    """Create instance, return ID."""
    result = subprocess.run(
        ["vastai", "create", "instance", str(offer_id),
         "--image", "pytorch/pytorch:2.5.1-cuda12.4-cudnn9-devel",
         "--disk", str(disk),
         "--ssh", "--direct",
         "--onstart-cmd", onstart],
        capture_output=True, text=True,
    )
    output = result.stdout.strip()
    # Output format: "Started. {'success': True, 'new_contract': 12345, ...}"
    # Extract the dict part after "Started. " — it uses Python dict syntax, not JSON
    import ast
    try:
        # Find the dict in the output
        brace_start = output.index("{")
        dict_str = output[brace_start:]
        data = ast.literal_eval(dict_str)
        if not data.get("success", False):
            return None
        return int(data.get("new_contract", data.get("id", 0)))
    except (ValueError, SyntaxError):
        if result.returncode != 0:
            print(f"  Create failed: {result.stderr.strip()}", file=sys.stderr)
        return None


def get_results(sweep_id: str) -> list[dict]:
    """Download and display sweep results from R2."""
    result = subprocess.run(
        ["aws", "s3", "ls", f"s3://{R2_BUCKET}/sweeps/{sweep_id}/",
         "--endpoint-url", R2_ENDPOINT, "--recursive"],
        capture_output=True, text=True,
    )
    metrics_files = [
        line.split()[-1] for line in result.stdout.strip().split("\n")
        if line.strip().endswith("metrics.json")
    ]

    results = []
    for mf in metrics_files:
        dl = subprocess.run(
            ["aws", "s3", "cp", f"s3://{R2_BUCKET}/{mf}", "/dev/stdout",
             "--endpoint-url", R2_ENDPOINT],
            capture_output=True, text=True,
        )
        if dl.returncode == 0:
            try:
                results.append(json.loads(dl.stdout))
            except json.JSONDecodeError:
                pass

    return sorted(results, key=lambda r: r.get("validation_value_correlation", 0), reverse=True)


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Parallel hyperparam sweep on vast.ai")
    p.add_argument("--gpu", default="A100_SXM4")
    p.add_argument("--max-price", type=float, default=1.00)
    p.add_argument("--sweep-id", default=None)
    p.add_argument("--dry-run", action="store_true")
    p.add_argument("--results", type=str, default=None, help="Fetch results for a sweep ID")
    p.add_argument("--configs", type=str, default=None, help="JSON file with custom sweep configs")
    return p.parse_args()


def main() -> None:
    args = parse_args()
    sweep_id = args.sweep_id or f"sweep_{int(time.time())}"

    # Fetch results mode
    if args.results:
        results = get_results(args.results)
        if not results:
            print(f"No results found for sweep {args.results}")
            return
        print(f"\n{'Config':<12} {'val_corr':>10} {'val_mae':>10} {'policy_acc':>10} {'LR':>8} {'BS':>6} {'WD':>8}")
        print("-" * 72)
        for r in results:
            print(f"{r.get('sweep_config', '?'):<12} "
                  f"{r.get('validation_value_correlation', 0):>10.4f} "
                  f"{r.get('validation_value_mae', 0):>10.4f} "
                  f"{r.get('validation_policy_accuracy', 0):>10.4f} "
                  f"{r.get('sweep_lr', '?'):>8} "
                  f"{r.get('sweep_bs', '?'):>6} "
                  f"{r.get('sweep_wd', '?'):>8}")
        best = results[0]
        print(f"\nBest: {best.get('sweep_config')} — val_corr={best.get('validation_value_correlation', 0):.4f}")
        return

    # Load custom configs or use defaults
    configs = SWEEP_CONFIGS
    if args.configs:
        configs = json.loads(open(args.configs).read())

    r2_access = os.environ.get("R2_ACCESS_KEY_ID", "")
    r2_secret = os.environ.get("R2_SECRET_ACCESS_KEY", "")
    if not r2_access or not r2_secret:
        print("ERROR: R2_ACCESS_KEY_ID and R2_SECRET_ACCESS_KEY required", file=sys.stderr)
        sys.exit(1)

    print(f"=== Hyperparam Sweep: {sweep_id} ===")
    print(f"GPU: {args.gpu}, max ${args.max_price}/hr")
    print(f"Configs: {len(configs)}")
    est_cost = len(configs) * args.max_price * 1.5  # ~1.5hrs per trial
    print(f"Est. cost: ~${est_cost:.1f} ({len(configs)} × ~${ args.max_price * 1.5:.2f})")
    print()
    for c in configs:
        tag = "← baseline" if c["name"] == "lr0.02" else ""
        print(f"  {c['name']:<12} lr={c['learning_rate']:<8} bs={c['batch_size']:<6} wd={c['weight_decay']:<8} {tag}")
    print()

    if args.dry_run:
        print("Dry run — no instances created.")
        print(f"\nTo launch: python3 -m tools.sweep_vastai --sweep-id {sweep_id}")
        return

    # Search for offers
    print(f"Searching for {len(configs)} {args.gpu} offers...", flush=True)
    offers = search_offers(args.gpu, args.max_price, len(configs) + 2)  # +2 buffer
    if len(offers) < len(configs):
        print(f"WARNING: Only {len(offers)} offers found, need {len(configs)}", file=sys.stderr)
        if not offers:
            sys.exit(1)

    # Upload scripts to R2 and launch instances
    print("Uploading training scripts to R2...", flush=True)
    instances = []
    used_offers: set[int] = set()
    for config in configs:
        # Upload full script to R2
        full_script = _build_onstart(config, sweep_id, r2_access, r2_secret)
        s3_path = upload_script_to_r2(full_script, sweep_id, config["name"], r2_access, r2_secret)

        # Short bootstrap that downloads and runs the full script
        bootstrap = (
            f"pip install -q awscli && "
            f"aws configure set aws_access_key_id {r2_access} && "
            f"aws configure set aws_secret_access_key {r2_secret} && "
            f"aws configure set default.region auto && "
            f"aws s3 cp s3://{R2_BUCKET}/{s3_path} /workspace/run.sh "
            f"--endpoint-url {R2_ENDPOINT} && "
            f"chmod +x /workspace/run.sh && "
            f"bash /workspace/run.sh"
        )

        # Try offers until one works
        launched = False
        for offer in offers:
            offer_id = offer["id"]
            if offer_id in used_offers:
                continue
            price = offer.get("dph_total", offer.get("dph", "?"))
            instance_id = create_instance(offer_id, bootstrap)
            if instance_id is not None:
                used_offers.add(offer_id)
                instances.append({"id": instance_id, "config": config["name"], "offer": offer_id, "price": price})
                print(f"  Launched: {config['name']:<12} → instance #{instance_id} (${price}/hr)", flush=True)
                launched = True
                time.sleep(1)
                break
        if not launched:
            print(f"  FAILED: {config['name']} — no offers available", flush=True)

    print(f"\n=== {len(instances)} instances launched ===")
    print(f"Sweep ID: {sweep_id}")
    print(f"\nMonitor:")
    print(f"  vastai show instances")
    for inst in instances:
        print(f"  vastai logs {inst['id']} --tail 20  # {inst['config']}")
    print(f"\nCheck results (~70 min):")
    print(f"  python3 -m tools.sweep_vastai --results {sweep_id}")
    print(f"\nCleanup:")
    for inst in instances:
        print(f"  vastai destroy instance {inst['id']}")


if __name__ == "__main__":
    main()
