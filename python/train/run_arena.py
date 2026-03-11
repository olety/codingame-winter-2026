from __future__ import annotations

import argparse
import json
import os
import subprocess
from pathlib import Path

from python.train.java_smoke import run_java_smoke
from python.train.results import append_result, check_gates, compute_composite


REPO_ROOT = Path(__file__).resolve().parents[2]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--candidate-config",
        type=Path,
        default=REPO_ROOT / "rust/bot/configs/default_search_v2.json",
    )
    parser.add_argument(
        "--incumbent-config",
        type=Path,
        default=REPO_ROOT / "rust/bot/configs/anchor_search_v1.json",
    )
    parser.add_argument(
        "--anchor-config",
        type=Path,
        default=REPO_ROOT / "rust/bot/configs/anchor_search_v1.json",
    )
    parser.add_argument(
        "--heldout-suite",
        type=Path,
        default=REPO_ROOT / "config/arena/heldout_v1.txt",
    )
    parser.add_argument(
        "--shadow-suite",
        type=Path,
        default=REPO_ROOT / "config/arena/shadow_v1.txt",
    )
    parser.add_argument("--league", type=int, default=4)
    parser.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 1) - 2))
    parser.add_argument("--results-db", type=Path, default=REPO_ROOT / "python/train/results.sqlite")
    parser.add_argument("--name", type=str, default="arena_eval")
    return parser.parse_args()


def run(cmd: list[str]) -> None:
    subprocess.run(cmd, cwd=REPO_ROOT, check=True)


def build_release_arena() -> Path:
    run(["cargo", "build", "--release", "-q", "-p", "snakebot-bot", "--bin", "arena"])
    return REPO_ROOT / "target/release/arena"


def run_arena_binary(
    arena_bin: Path,
    candidate_config: Path,
    opponent_config: Path,
    suite: Path,
    league: int,
    jobs: int,
) -> dict:
    output = subprocess.check_output(
        [
            str(arena_bin),
            "--bot-a-config",
            str(candidate_config),
            "--bot-b-config",
            str(opponent_config),
            "--suite",
            str(suite),
            "--league",
            str(league),
            "--jobs",
            str(jobs),
        ],
        cwd=REPO_ROOT,
        text=True,
    )
    return json.loads(output)


def main() -> None:
    args = parse_args()
    arena_bin = build_release_arena()
    heldout = run_arena_binary(
        arena_bin,
        args.candidate_config,
        args.incumbent_config,
        args.heldout_suite,
        args.league,
        args.jobs,
    )
    shadow = run_arena_binary(
        arena_bin,
        args.candidate_config,
        args.anchor_config,
        args.shadow_suite,
        args.league,
        args.jobs,
    )
    smoke = run_java_smoke(
        league=args.league,
        seed_file=REPO_ROOT / "config/arena/smoke_v1.txt",
        boss_count=4,
        mirror_count=4,
    )

    heldout_win_margin = (heldout["wins"] - heldout["losses"]) / max(heldout["matches"], 1)
    metrics = {
        "heldout_body_diff": heldout["average_body_diff"],
        "heldout_win_margin": heldout_win_margin,
        "shadow_body_diff": shadow["average_body_diff"],
        "turn_p99_ms": heldout["side_a"]["move_time_p99_ms"],
        "java_smoke_passed": float(smoke["passed"]),
    }
    gate_result = check_gates(metrics)
    status = (
        "accepted"
        if heldout["average_body_diff"] > 0.0
        and heldout["wins"] >= heldout["losses"]
        and shadow["average_body_diff"] >= 0.0
        and smoke["passed"]
        and gate_result.passed
        else "rejected"
    )

    append_result(
        args.results_db,
        name=args.name,
        status=status,
        description="arena + java smoke evaluation",
        metrics=metrics,
        failures=gate_result.failures,
    )
    payload = {
        "status": status,
        "composite_score": compute_composite(metrics),
        "failures": gate_result.failures,
        "metrics": metrics,
        "heldout": heldout,
        "shadow": shadow,
        "java_smoke": smoke,
    }
    print(json.dumps(payload, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
