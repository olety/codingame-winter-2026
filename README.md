# Codingame Winter 2026 Bot

This repository contains a local development stack for the CodinGame Winter Challenge 2026 snakebot game.

Current status:

- Java-parity Rust simulator
- Rust contest bot with turn-aware search budgets
- release-mode Rust arena harness with frozen seed suites
- Java-referee smoke canary for real I/O and reconciliation checks
- deterministic self-play export and local Python value-training pipeline

## Repository layout

- `rust/engine`: game state, rules, map generation, oracle loading, and Java parity tools
- `rust/bot`: contest bot, search, evaluator, arena binary, and self-play export
- `python/train`: local self-play orchestration, smoke/arena runners, value training, and results ledger
- `src/test/java/com/codingame/game`: Java oracle and real referee runner helpers
- `docs/memory`: repo-local status and handoff notes

## Quick checks

Run the Rust test suite:

```bash
cargo test --workspace
```

Run the Rust-vs-Java parity sweep:

```bash
cargo run -q -p snakebot-engine --bin java_diff -- --init-source rust --seeds 50 --turns 20
```

Run the Java smoke canary:

```bash
python3 -m python.train.java_smoke --boss-count 1 --mirror-count 1
```

Run the Rust arena harness:

```bash
python3 -m python.train.run_arena \
  --candidate-config rust/bot/configs/default_search_v2.json \
  --incumbent-config rust/bot/configs/anchor_search_v1.json \
  --anchor-config rust/bot/configs/anchor_search_v1.json
```

## Self-play and training

The self-play/export/training workflow is documented in:

- [python/train/README.md](./python/train/README.md)

That includes:

- Java map dumps
- release-mode self-play export
- parallel local data generation
- grouped/deduped value-model training

## Project memory

The best starting point for project status and handoff notes is:

- [docs/memory/README.md](./docs/memory/README.md)

That index links to the short status note and the more detailed engine/search and training docs.

## Git remotes

- `origin`: `https://github.com/oneiron-dev/codingame-winter-2026.git`
- `upstream`: `https://github.com/CodinGame/WinterChallenge2026-Exotec.git`
