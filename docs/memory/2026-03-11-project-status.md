# WinterChallenge 2026 Project Status

Date: 2026-03-11

## Summary

The repository now contains a working Rust simulator, a working Rust contest bot, Java oracle tooling for referee comparison, a Python local value-training stack, and a parallel local self-play pipeline for Apple Silicon.

The most important current result is that the Rust simulator now matches the Java referee on a substantial differential check:

```bash
cargo run -q -p snakebot-engine --bin java_diff -- --init-source rust --seeds 50 --turns 20
```

That passed at the time of writing.

## Local Commits Added

- `f00bb51` `Add Rust simulator, bot, and oracle tooling`
- `d0a63f8` `Fix Java parity in simulator state updates`
- `8b811b1` `Add parallel self-play data pipeline`

## Current Repo Layout

### Rust engine

Located under `rust/engine`.

Main files:

- `rust/engine/src/state.rs`
- `rust/engine/src/mapgen.rs`
- `rust/engine/src/java_random.rs`
- `rust/engine/src/oracle.rs`
- `rust/engine/src/bin/java_diff.rs`
- `rust/engine/src/bin/map_stage_dump.rs`

What it does:

- Represents the full game state.
- Implements movement, apple eating, beheading, and gravity.
- Generates maps using a Java-compatible RNG.
- Loads Java-dumped initial states.
- Compares Rust and Java frame by frame.

### Rust bot

Located under `rust/bot`.

Main files:

- `rust/bot/src/main.rs`
- `rust/bot/src/search.rs`
- `rust/bot/src/eval.rs`
- `rust/bot/src/features.rs`
- `rust/bot/src/selfplay.rs`
- `rust/bot/src/bin/selfplay_export.rs`

What it does:

- Parses CodinGame input.
- Reconciles visible frames with simulated state.
- Runs a maximin-style root search.
- Scores states with a hand-built evaluator.
- Encodes owner-relative features for ML.
- Exports self-play samples to JSONL.

### Python training stack

Located under `python/train`.

Main files:

- `python/train/dataset.py`
- `python/train/model.py`
- `python/train/train_value.py`
- `python/train/results.py`
- `python/train/run_local_experiment.py`
- `python/train/parallel_selfplay.py`
- `python/train/README.md`

What it does:

- Loads JSONL self-play data.
- Trains a small CNN value model.
- Prefers `mps` on Apple Silicon.
- Stores experiment results in SQLite.
- Can orchestrate Java map dumping, parallel Rust self-play export, shard merge, and optional training.

### Java oracle tools

Located under `src/test/java/com/codingame/game`.

Main files:

- `OracleSupport.java`
- `OracleCli.java`
- `MapDumpCli.java`
- `MapStageDumpCli.java`

What they do:

- Build real Java referee states.
- Replay scripted turns under the Java referee.
- Dump exact Java initial states.
- Dump intermediate map generation stages for debugging.

## Important Fixes Already Made

### 1. Java map generation parity

The Rust map generator originally diverged from Java on some seeds.

Root cause:

- `java.util.Random.nextInt(origin, bound)` was not matched correctly.
- The `sink lowest island` phase used a different sampled `lowerBy`.

Fix:

- `rust/engine/src/java_random.rs` now matches Java bounded range behavior.

Effect:

- Initial-state diffing now passes across a broader seed sweep.

### 2. Turn direction semantics

The Rust simulator originally persisted bird direction across turns.
The Java referee does not do that. It resets bird direction each turn in `Player.reset()`.

Root cause:

- Rust carried forward last turn's command direction.
- Java resets to `null`, then recomputes movement from current body facing unless a new command is given.

Fix:

- `rust/engine/src/state.rs` resets live-bird direction at the start of each turn.
- `rust/engine/src/mapgen.rs` no longer initializes birds with a baked-in stored direction.

Effect:

- A concrete replay mismatch at seed `7`, turn `2` was resolved.

## Current Verification Status

### Passed

- `cargo test --workspace`
- `mvn -q -DskipTests test-compile`
- Java map dumping through `MapDumpCli`
- Rust loading of Java dump records
- End-to-end Rust self-play export to JSONL
- Python training on exported data using `mps`
- Parallel local self-play smoke run with 2 workers
- Rust vs Java differential replay for 50 seeds over 20 turns

### Example commands that worked

```bash
cargo test --workspace
```

```bash
cargo run -q -p snakebot-engine --bin java_diff -- --init-source rust --seeds 50 --turns 20
```

```bash
java -cp "target/classes:target/test-classes:$(cat cp.txt)" \
  com.codingame.game.MapDumpCli 1 1000 4 > python/train/artifacts/maps_l4.jsonl
```

```bash
python3 -m python.train.parallel_selfplay \
  --seed-count 4 \
  --workers 2 \
  --games 4 \
  --max-turns 20 \
  --search-ms 2 \
  --train
```

## Current Performance/Workflow Status

### What works well now

- The simulator is good enough to use as the main local environment.
- Java oracle dumps are available if exact referee-generated initial states are needed.
- The local data pipeline can use CPU parallelism for self-play generation.
- Training can run locally on `mps`.

### What is still basic

- The search bot is functional but still shallow.
- The evaluator is hand-built and not yet strongly tuned.
- The ML side is still a small value-only model.
- There is no full held-out Elo harness yet.
- The results ledger anticipates stronger evaluation metrics than are currently populated.

## Recommended Near-Term Next Steps

### 1. Generate a real self-play corpus

Use the parallel pipeline to create a larger dataset:

```bash
python -m python.train.parallel_selfplay \
  --seed-count 512 \
  --workers 8 \
  --games 512 \
  --max-turns 120 \
  --search-ms 2 \
  --train
```

### 2. Add a held-out match harness

Track real bot quality, not just training loss:

- held-out body differential
- held-out win rate
- per-turn runtime percentiles

### 3. Improve the Rust search bot

Likely highest leverage areas:

- deeper root search under time budget
- better opponent action pruning
- better leaf evaluation
- tactical tests around food races, self-support, and beheading opportunities

### 4. Retrain after better search data

The current ML pipeline is ready, but the biggest gains should come from better self-play/search targets.

## Notes

- `.serena/` exists locally and is untracked. It was intentionally not committed.
- `python/train/artifacts/` and `python/train/data/` are being used for local generated assets and are ignored.
- This file is intended to serve as the repo-local project memory/status note.
