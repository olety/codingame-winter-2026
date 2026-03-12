from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import modal


REPO_ROOT = Path(__file__).resolve().parents[3]
REMOTE_REPO = "/root/repo"

image = (
    modal.Image.debian_slim(python_version="3.11")
    .pip_install("torch")
)
app = modal.App("snakebot-outerloop")


@app.function(
    image=image,
    mounts=[modal.Mount.from_local_dir(str(REPO_ROOT), remote_path=REMOTE_REPO)],
    timeout=60 * 60,
)
def run_task(task: str, spec_json: str) -> dict:
    sys.path.insert(0, REMOTE_REPO)
    spec = json.loads(spec_json)
    if task == "train":
        from python.train.outerloop.export_weights import export_weights
        from python.train.outerloop.train_model import train_from_spec

        metrics = train_from_spec(spec)
        output_dir = Path(spec["output_dir"])
        weights_path = output_dir / "hybrid_weights.json"
        weights_payload = export_weights(
            Path(metrics["model_path"]),
            Path(metrics["training_config_path"]),
            weights_path,
        )
        return {
            "task": task,
            "metrics": metrics,
            "weights_json": json.dumps(weights_payload, indent=2, sort_keys=True),
        }
    if task == "selfplay":
        import subprocess

        output = subprocess.check_output(
            [
                "python3",
                "-m",
                "python.train.parallel_selfplay",
                "--seed-start",
                str(spec["seed_start"]),
                "--seed-count",
                str(spec["seed_count"]),
                "--league",
                str(spec["league"]),
                "--workers",
                str(spec["workers"]),
                "--max-turns",
                str(spec["max_turns"]),
                "--extra-nodes-after-root",
                str(spec["extra_nodes_after_root"]),
                "--config-path",
                str(spec["config_path"]),
                "--dataset-path",
                str(spec["dataset_path"]),
                "--output-dir",
                str(spec["output_dir"]),
            ],
            cwd=REMOTE_REPO,
            text=True,
        )
        return json.loads(output)
    raise ValueError(f"unsupported modal task: {task}")


@app.local_entrypoint()
def main(task: str, spec_json: str) -> None:
    print(json.dumps(run_task.remote(task, spec_json), indent=2, sort_keys=True))
