from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
from copy import deepcopy
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[2]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--base-config",
        type=Path,
        default=REPO_ROOT / "rust/bot/configs/submission_current.json",
    )
    parser.add_argument(
        "--incumbent-config",
        type=Path,
        default=REPO_ROOT / "rust/bot/configs/incumbent_current.json",
    )
    parser.add_argument(
        "--anchor-config",
        type=Path,
        default=REPO_ROOT / "rust/bot/configs/anchor_root_only.json",
    )
    parser.add_argument(
        "--smoke-suite",
        type=Path,
        default=REPO_ROOT / "config/arena/smoke_v1.txt",
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
    parser.add_argument(
        "--results-dir",
        type=Path,
        default=REPO_ROOT / "python/train/artifacts/search_sweeps",
    )
    parser.add_argument("--run-name", type=str, default="search_sweep_v1")
    parser.add_argument("--results-db", type=Path, default=REPO_ROOT / "python/train/results.sqlite")
    parser.add_argument("--top-my-values", type=str, default="4,6,8")
    parser.add_argument("--top-opp-values", type=str, default="4,6,8")
    parser.add_argument("--child-my-values", type=str, default="3,4,5")
    parser.add_argument("--child-opp-values", type=str, default="3,4,5")
    parser.add_argument("--later-turn-values", type=str, default="38,40,42")
    parser.add_argument("--smoke-top-k", type=int, default=6)
    parser.add_argument("--promote", action="store_true")
    return parser.parse_args()


def parse_int_list(raw: str) -> list[int]:
    return [int(part.strip()) for part in raw.split(",") if part.strip()]


def load_config(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def dump_config(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def run_arena(
    candidate_config: Path,
    args: argparse.Namespace,
    *,
    heldout_suite: Path,
    shadow_suite: Path,
    name: str,
    skip_java_smoke: bool,
) -> dict[str, Any]:
    cmd = [
        "python3",
        "-m",
        "python.train.run_arena",
        "--candidate-config",
        str(candidate_config),
        "--incumbent-config",
        str(args.incumbent_config),
        "--anchor-config",
        str(args.anchor_config),
        "--heldout-suite",
        str(heldout_suite),
        "--shadow-suite",
        str(shadow_suite),
        "--league",
        str(args.league),
        "--jobs",
        str(args.jobs),
        "--results-db",
        str(args.results_db),
        "--name",
        name,
    ]
    if skip_java_smoke:
        cmd.append("--skip-java-smoke")
    output = subprocess.check_output(
        cmd,
        cwd=REPO_ROOT,
        text=True,
    )
    return json.loads(output)


def sort_key(result: dict[str, Any]) -> tuple[float, float, float, float]:
    metrics = result.get("metrics", {})
    return (
        float(result.get("composite_score", 0.0)),
        float(metrics.get("heldout_body_diff", 0.0)),
        float(metrics.get("heldout_win_margin", 0.0)),
        float(metrics.get("heldout_tiebreak_win_rate", 0.0)),
    )


def stage_topology_candidates(base_config: dict[str, Any], args: argparse.Namespace, candidate_dir: Path) -> list[Path]:
    later_turn_ms = int(base_config["search"]["later_turn_ms"])
    candidates: list[Path] = []
    for top_my in parse_int_list(args.top_my_values):
        for top_opp in parse_int_list(args.top_opp_values):
            for child_my in parse_int_list(args.child_my_values):
                for child_opp in parse_int_list(args.child_opp_values):
                    payload = deepcopy(base_config)
                    payload["name"] = (
                        f"sweep_tmy{top_my}_topp{top_opp}_cmy{child_my}_copp{child_opp}_lat{later_turn_ms}"
                    )
                    payload["search"]["deepen_top_my"] = top_my
                    payload["search"]["deepen_top_opp"] = top_opp
                    payload["search"]["deepen_child_my"] = child_my
                    payload["search"]["deepen_child_opp"] = child_opp
                    payload["search"]["later_turn_ms"] = later_turn_ms
                    path = candidate_dir / "stage1" / f"{payload['name']}.json"
                    dump_config(path, payload)
                    candidates.append(path)
    return candidates


def expand_finalists(stage1_results: list[dict[str, Any]], base_config: dict[str, Any], args: argparse.Namespace, candidate_dir: Path) -> list[Path]:
    finalists: list[Path] = []
    seen_behaviors: set[str] = set()
    ranked = sorted(stage1_results, key=sort_key, reverse=True)
    for result in ranked:
        if result.get("status") == "informational":
            continue
        behavior_hash = str(result.get("metrics", {}).get("candidate_config_behavior_hash", ""))
        if not behavior_hash or behavior_hash in seen_behaviors:
            continue
        seen_behaviors.add(behavior_hash)
        base_candidate = Path(result["candidate_config"])
        payload = load_config(base_candidate)
        for later_turn_ms in parse_int_list(args.later_turn_values):
            variant = deepcopy(payload)
            variant["name"] = (
                f"{payload['name']}_lat{later_turn_ms}"
                if not payload["name"].endswith(f"_lat{later_turn_ms}")
                else payload["name"]
            )
            variant["search"]["later_turn_ms"] = later_turn_ms
            path = candidate_dir / "stage2" / f"{variant['name']}.json"
            dump_config(path, variant)
            finalists.append(path)
        if len(seen_behaviors) >= args.smoke_top_k:
            break
    return finalists


def promote_winner(winner_path: Path, submission_path: Path, incumbent_path: Path) -> None:
    previous_submission = submission_path.read_text(encoding="utf-8")
    incumbent_path.write_text(previous_submission, encoding="utf-8")
    submission_path.write_text(winner_path.read_text(encoding="utf-8"), encoding="utf-8")


def main() -> None:
    args = parse_args()
    run_dir = args.results_dir / args.run_name
    if run_dir.exists():
        shutil.rmtree(run_dir)
    candidate_dir = run_dir / "candidates"
    summary_path = run_dir / "summary.json"

    base_config = load_config(args.base_config)
    stage1_candidates = stage_topology_candidates(base_config, args, candidate_dir)

    stage1_results: list[dict[str, Any]] = []
    for index, candidate in enumerate(stage1_candidates):
        result = run_arena(
            candidate,
            args,
            heldout_suite=args.smoke_suite,
            shadow_suite=args.smoke_suite,
            name=f"{args.run_name}_stage1_{index:03d}",
            skip_java_smoke=True,
        )
        result["candidate_config"] = str(candidate)
        stage1_results.append(result)

    stage2_candidates = expand_finalists(stage1_results, base_config, args, candidate_dir)
    stage2_results: list[dict[str, Any]] = []
    for index, candidate in enumerate(stage2_candidates):
        result = run_arena(
            candidate,
            args,
            heldout_suite=args.heldout_suite,
            shadow_suite=args.shadow_suite,
            name=f"{args.run_name}_stage2_{index:03d}",
            skip_java_smoke=False,
        )
        result["candidate_config"] = str(candidate)
        stage2_results.append(result)

    accepted = [result for result in stage2_results if result.get("status") == "accepted"]
    best_result = max(accepted or stage2_results or stage1_results, key=sort_key)

    promoted = False
    if args.promote and accepted:
        promote_winner(
            Path(best_result["candidate_config"]),
            REPO_ROOT / "rust/bot/configs/submission_current.json",
            REPO_ROOT / "rust/bot/configs/incumbent_current.json",
        )
        promoted = True

    summary = {
        "run_name": args.run_name,
        "base_config": str(args.base_config),
        "incumbent_config": str(args.incumbent_config),
        "anchor_config": str(args.anchor_config),
        "smoke_suite": str(args.smoke_suite),
        "heldout_suite": str(args.heldout_suite),
        "shadow_suite": str(args.shadow_suite),
        "stage1_candidates": len(stage1_candidates),
        "stage2_candidates": len(stage2_candidates),
        "accepted_candidates": len(accepted),
        "best_result": best_result,
        "promoted": promoted,
        "stage1_results": stage1_results,
        "stage2_results": stage2_results,
    }
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    summary_path.write_text(json.dumps(summary, indent=2, sort_keys=True), encoding="utf-8")
    print(json.dumps(summary, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
