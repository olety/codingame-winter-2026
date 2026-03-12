from __future__ import annotations

import argparse
import json
import shutil
import subprocess
from pathlib import Path

from python.train.outerloop.registry import load_json, mark_promoted


REPO_ROOT = Path(__file__).resolve().parents[3]


def promote(stage2_path: Path) -> dict:
    payload = load_json(stage2_path)
    if not payload.get("promotable"):
        raise RuntimeError("candidate is not promotable")
    run_id = payload["run_id"]
    candidate_id = payload["candidate_id"]
    candidate_dir = Path(payload["candidate_dir"])
    candidate_config = candidate_dir / "candidate_config.json"
    submission = REPO_ROOT / "rust/bot/configs/submission_current.json"
    incumbent = REPO_ROOT / "rust/bot/configs/incumbent_current.json"
    archive_dir = REPO_ROOT / "rust/bot/configs/archive"
    archive_dir.mkdir(parents=True, exist_ok=True)
    archived_submission = archive_dir / f"{candidate_id}.json"

    shutil.copy2(submission, incumbent)
    shutil.copy2(candidate_config, submission)
    shutil.copy2(candidate_config, archived_submission)
    subprocess.run(
        ["python3", "tools/generate_flattened_submission.py", "--config", str(submission)],
        cwd=REPO_ROOT,
        check=True,
    )
    mark_promoted(run_id, candidate_id)
    return {
        "run_id": run_id,
        "candidate_id": candidate_id,
        "submission": str(submission),
        "incumbent": str(incumbent),
        "archived": str(archived_submission),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stage2", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(promote(args.stage2), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
