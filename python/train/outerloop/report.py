from __future__ import annotations

import json
from pathlib import Path

from python.train.outerloop.registry import load_json


def summarize_run(manifest_path: str | Path) -> dict:
    manifest = load_json(manifest_path)
    candidates = manifest.get("candidates", {})
    stage2 = []
    for candidate in candidates.values():
        stage_path = candidate.get("stages", {}).get("stage2")
        if stage_path:
            stage2.append(load_json(stage_path))
    return {
        "run_id": manifest["run_id"],
        "status": manifest["status"],
        "candidate_count": len(candidates),
        "promoted_candidate_id": manifest.get("promoted_candidate_id"),
        "authoritative_candidates": len(stage2),
    }


def main() -> None:
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    args = parser.parse_args()
    print(json.dumps(summarize_run(args.manifest), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
