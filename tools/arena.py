#!/usr/bin/env python3
"""Fast parallel arena: run two bots against each other via Java referee."""
import argparse, json, subprocess, sys
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CP_FILE = ROOT / "cp.txt"

def get_classpath():
    if not CP_FILE.exists():
        subprocess.run(["mvn", "-q", "dependency:build-classpath", f"-Dmdep.outputFile={CP_FILE}"],
                       cwd=ROOT, check=True)
    return f"target/classes:target/test-classes:{CP_FILE.read_text().strip()}"

def run_seed(args):
    seed, cp, agent_a, agent_b, league = args
    try:
        r = subprocess.run(
            ["java", "-cp", cp, "com.codingame.game.RunnerCli",
             "--league", str(league), "--seed", str(seed),
             "--agent-a", agent_a, "--agent-b", agent_b],
            capture_output=True, text=True, timeout=120, cwd=str(ROOT))
        # Last line is the JSON
        for line in reversed(r.stdout.strip().split("\n")):
            line = line.strip()
            if line.startswith("{"):
                d = json.loads(line)
                return seed, d["scores"]["0"], d["scores"]["1"], None
        return seed, 0, 0, "no JSON output"
    except Exception as e:
        return seed, 0, 0, str(e)

def main():
    p = argparse.ArgumentParser()
    p.add_argument("-a", "--agent-a", required=True)
    p.add_argument("-b", "--agent-b", required=True)
    p.add_argument("-n", "--seeds", type=int, default=60)
    p.add_argument("-s", "--start-seed", type=int, default=1)
    p.add_argument("-j", "--jobs", type=int, default=8)
    p.add_argument("-l", "--league", type=int, default=4)
    args = p.parse_args()

    cp = get_classpath()
    seeds = range(args.start_seed, args.start_seed + args.seeds)
    tasks = [(s, cp, args.agent_a, args.agent_b, args.league) for s in seeds]

    wins = losses = draws = errors = 0
    total_diff = 0
    results = []

    with ProcessPoolExecutor(max_workers=args.jobs) as ex:
        futs = {ex.submit(run_seed, t): t[0] for t in tasks}
        for f in as_completed(futs):
            seed, sa, sb, err = f.result()
            if err:
                errors += 1
                print(f"  Seed {seed}: ERROR ({err})")
                continue
            diff = sa - sb
            total_diff += diff
            results.append((seed, sa, sb))
            if sa > sb:
                wins += 1
            elif sa < sb:
                losses += 1
            else:
                draws += 1

    n = wins + losses + draws
    avg = total_diff / n if n > 0 else 0
    print(f"\n=== {args.agent_a} vs {args.agent_b} ({n} games, {args.jobs} workers) ===")
    print(f"W-L-D: {wins}-{losses}-{draws}  avg_diff: {avg:+.1f}")
    if errors:
        print(f"Errors: {errors}")

if __name__ == "__main__":
    main()
