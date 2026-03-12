from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path


def launch_modal(task: str, spec: dict) -> dict:
    command = [
        "modal",
        "run",
        "python/train/outerloop/modal_job.py",
        "--task",
        task,
        "--spec-json",
        json.dumps(spec),
    ]
    output = subprocess.check_output(command, text=True)
    return json.loads(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--task", choices=("train", "selfplay"), required=True)
    parser.add_argument("--spec", type=Path, required=True)
    args = parser.parse_args()
    payload = json.loads(args.spec.read_text(encoding="utf-8"))
    result = launch_modal(args.task, payload)
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
