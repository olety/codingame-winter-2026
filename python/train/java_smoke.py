from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def run(cmd: list[str], *, stdout=None) -> None:
    subprocess.run(cmd, cwd=REPO_ROOT, check=True, stdout=stdout)


def ensure_java_oracle() -> Path:
    run(
        [
            "mvn",
            "-q",
            "-DskipTests",
            "test-compile",
            "dependency:build-classpath",
            "-Dmdep.outputFile=cp.txt",
        ]
    )
    classpath = (REPO_ROOT / "cp.txt").read_text(encoding="utf-8").strip()
    return Path(f"target/classes:target/test-classes:{classpath}")


def build_release_bot() -> Path:
    run(["cargo", "build", "--release", "-q", "-p", "snakebot-bot"])
    return REPO_ROOT / "target/release/snakebot-bot"


def load_seeds(path: Path, count: int) -> list[int]:
    seeds: list[int] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        seeds.append(int(line))
        if len(seeds) >= count:
            break
    return seeds


def run_java_smoke(
    *,
    league: int,
    seed_file: Path,
    boss_count: int,
    mirror_count: int,
) -> dict:
    classpath = ensure_java_oracle()
    bot_path = build_release_bot()
    strict_bot = f"env SNAKEBOT_STRICT_RECONCILE=1 {bot_path}"
    boss = "python3 config/Boss.py"
    seeds = load_seeds(seed_file, boss_count + mirror_count)
    if len(seeds) < boss_count + mirror_count:
        raise RuntimeError(f"not enough smoke seeds in {seed_file}")

    matches: list[dict] = []
    failed: list[dict] = []

    for seed in seeds[:boss_count]:
        payload = run_single_match(classpath, league, seed, strict_bot, boss)
        matches.append(payload)
        if not payload["passed"]:
            failed.append(payload)

    for seed in seeds[boss_count : boss_count + mirror_count]:
        payload = run_single_match(classpath, league, seed, strict_bot, strict_bot)
        matches.append(payload)
        if not payload["passed"]:
            failed.append(payload)

    return {
        "league": league,
        "matches": len(matches),
        "passed": not failed,
        "failures": failed,
        "results": matches,
    }


def run_single_match(classpath: Path, league: int, seed: int, agent_a: str, agent_b: str) -> dict:
    output = subprocess.check_output(
        [
            "java",
            "-cp",
            str(classpath),
            "com.codingame.game.RunnerCli",
            "--league",
            str(league),
            "--seed",
            str(seed),
            "--agent-a",
            agent_a,
            "--agent-b",
            agent_b,
        ],
        cwd=REPO_ROOT,
        text=True,
        stderr=subprocess.DEVNULL,
    )
    return json.loads(output)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--league", type=int, default=4)
    parser.add_argument(
        "--seed-file",
        type=Path,
        default=REPO_ROOT / "config/arena/smoke_v1.txt",
    )
    parser.add_argument("--boss-count", type=int, default=4)
    parser.add_argument("--mirror-count", type=int, default=4)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    payload = run_java_smoke(
        league=args.league,
        seed_file=args.seed_file,
        boss_count=args.boss_count,
        mirror_count=args.mirror_count,
    )
    print(json.dumps(payload, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
