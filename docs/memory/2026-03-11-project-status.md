# WinterChallenge 2026 Project Status

Date: 2026-03-11

## Summary

The repository now has:

- a Java-parity Rust simulator
- a stronger Rust search bot with corrected depth-2 refinement and embedded submission config
- a release-mode Rust arena harness with frozen seed suites
- a real Java-referee smoke canary for live I/O/reconciliation checks on the exact built candidate artifact
- a deterministic self-play/export path with Rust-generated seeds by default and a grouped Python value-training pipeline
- semantic behavior hashes alongside raw artifact hashes for config identity and promotion logic
- split opening/later-turn arena timing so only later turns are hard-gated
- a staged release-mode search sweep helper with smoke filtering and heldout/shadow finalist evaluation

The newest local commit after the original simulator/bot work is:

- `43d7d1c` `Add arena evaluation and deterministic search upgrades`
- `0eec4d7` `Tighten evaluation hashes and arena timing gates`
- plus the current uncommitted follow-up for staged sweep, tiebreak reporting, and frozen regression/hash fixtures

The most important current checks that passed are:

```bash
cargo run -q -p snakebot-engine --bin java_diff -- --init-source rust --seeds 50 --turns 20
```

```bash
python3 -m python.train.java_smoke --boss-count 1 --mirror-count 1
```

```bash
python3 -m python.train.run_arena --heldout-suite /tmp/arena_smoke_2.txt --shadow-suite /tmp/arena_smoke_2.txt --jobs 2 --name arena_smoke
```

```bash
python3 -m python.train.sweep_search --smoke-suite /tmp/arena_smoke_2.txt --heldout-suite /tmp/arena_smoke_2.txt --shadow-suite /tmp/arena_smoke_2.txt --top-my-values 4,6 --top-opp-values 6 --child-my-values 3 --child-opp-values 4 --later-turn-values 38,40 --smoke-top-k 2
```

## Read This Folder

Use [README.md](./README.md) as the index.

Detailed follow-up docs:

- [Engine, Search, and Evaluation Harness](./2026-03-11-engine-search-and-eval.md)
- [Training and Data Pipeline](./2026-03-11-training-and-data-pipeline.md)

## Local Commits Added

- `f00bb51` `Add Rust simulator, bot, and oracle tooling`
- `d0a63f8` `Fix Java parity in simulator state updates`
- `8b811b1` `Add parallel self-play data pipeline`
- `4f6d769` `Add project status memory note`
- `43d7d1c` `Add arena evaluation and deterministic search upgrades`
- `0eec4d7` `Tighten evaluation hashes and arena timing gates`

## Current Repo Shape

- Rust engine in `rust/engine`
- Rust bot/search/arena/export in `rust/bot`
- Python training/eval wrappers in `python/train`
- Java oracle and runner helpers in `src/test/java/com/codingame/game`

## What Changed Most Recently

- Engine semantics are split cleanly: natural Java-parity `is_game_over()` stays separate from contest `is_terminal(200)` and `final_result(200)`.
- Live search now uses an embedded submission config, while arena/self-play/training operate on explicit candidate/incumbent/anchor JSONs.
- Depth-2 refinement now recomputes refined root worst/mean scores and exported root values instead of only downgrading branches.
- Java smoke now rebuilds the candidate artifact, verifies the embedded artifact and behavior hashes, and then runs the referee canary on that exact build.
- Arena now distinguishes raw artifact identity from semantic behavior identity, short-circuits behavior self-matches, and records opening/later timing buckets separately.
- Arena results now carry heldout/shadow tiebreak win rates, and the search regression is frozen on a dedicated test config instead of the live embedded submission config.
- The repo now includes a staged release-mode sweep runner plus a shared Rust/Python hash fixture test so config hashing and sweep evaluation can evolve without drifting silently.
- Self-play now defaults to Rust-generated seeds, records explicit budget type/value plus both config hashes, and can train directly from shard directories without mandatory merging.

## Recommended Next Read

- Read [2026-03-11 Engine, Search, and Evaluation Harness](./2026-03-11-engine-search-and-eval.md) if you need to work on live strength or referee parity.
- Read [2026-03-11 Training and Data Pipeline](./2026-03-11-training-and-data-pipeline.md) if you need to work on export, datasets, or model training.
- The immediate next operational step is a real full-suite sweep followed by the first true promotion into `submission_current.json` and `incumbent_current.json`.

## Notes

- `.serena/` exists locally and is untracked. It was intentionally not committed.
- `python/train/artifacts/` and `python/train/data/` are being used for local generated assets and are ignored.
- This file is now the short status entry point, not the full detail dump.
