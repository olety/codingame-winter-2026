# WinterChallenge 2026 Project Status

Date: 2026-03-11

## Summary

The repository now has:

- a Java-parity Rust simulator
- a stronger Rust search bot with explicit contest terminal semantics
- a release-mode Rust arena harness with frozen seed suites
- a real Java-referee smoke canary for live I/O/reconciliation checks
- a deterministic self-play/export path and a grouped Python value-training pipeline

The newest local commit after the original simulator/bot work is:

- `43d7d1c` `Add arena evaluation and deterministic search upgrades`

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

## Current Repo Shape

- Rust engine in `rust/engine`
- Rust bot/search/arena/export in `rust/bot`
- Python training/eval wrappers in `python/train`
- Java oracle and runner helpers in `src/test/java/com/codingame/game`

## What Changed Most Recently

- Engine semantics are split cleanly: natural Java-parity `is_game_over()` stays separate from contest `is_terminal(200)` and `final_result(200)`.
- Live search now uses turn-aware budgets, while offline export uses deterministic post-root node budgets.
- The arena and Java smoke harnesses are now the main evaluation path, with training-only runs marked informational.
- Self-play rows now include stable hashes, search stats, and compact root action/value targets.

## Recommended Next Read

- Read [2026-03-11 Engine, Search, and Evaluation Harness](./2026-03-11-engine-search-and-eval.md) if you need to work on live strength or referee parity.
- Read [2026-03-11 Training and Data Pipeline](./2026-03-11-training-and-data-pipeline.md) if you need to work on export, datasets, or model training.

## Notes

- `.serena/` exists locally and is untracked. It was intentionally not committed.
- `python/train/artifacts/` and `python/train/data/` are being used for local generated assets and are ignored.
- This file is now the short status entry point, not the full detail dump.
