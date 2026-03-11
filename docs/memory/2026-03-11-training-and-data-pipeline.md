# 2026-03-11 Training and Data Pipeline

## Current export path

The self-play export path is now release-mode and deterministic by default.

Main pieces:

- `rust/bot/src/bin/selfplay_export.rs`
- `rust/bot/src/selfplay.rs`
- `python/train/parallel_selfplay.py`

Key behavior:

- Java map dumps are still available, but the hot loop can use Rust-generated states.
- Offline export now defaults to `extra_nodes_after_root` instead of wall-clock search.
- Release binaries are used for self-play/export instead of debug builds.

## Training row schema

Rows now include more than just tensors and a scalar value target.

Important fields:

- schema version
- git SHA
- config hash
- seed
- game id
- turn
- owner
- raw state hash
- encoded-view hash
- chosen joint-action id
- joint-action count
- aligned root value vector
- search stats

This is meant to support:

- grouped splits
- dedup checks
- later policy distillation

## Training split and dedup

The Python training path now:

- deduplicates by encoded-view hash
- groups by `(seed, game_id)`
- splits by grouped games instead of random rows

This is a guard against the earlier leakage issue where validation rows duplicated training positions.

## Results ledger

`python/train/results.py` is still the local SQLite ledger, but its meaning is now stricter:

- training-only runs are `informational`
- arena plus Java smoke is the acceptance gate

Wrapper scripts:

- `python/train/run_local_experiment.py`
- `python/train/run_arena.py`

## Recent smoke runs

Examples that passed after the pipeline update:

```bash
python3 -m python.train.parallel_selfplay \
  --seed-count 4 \
  --workers 2 \
  --games 4 \
  --max-turns 20 \
  --extra-nodes-after-root 64 \
  --train
```

That exported `160` rows and completed an `mps` training smoke.

## Current caveats

- Small smoke datasets can still produce tiny validation sets, so training metrics from those runs are only sanity checks.
- The current network is still value-only and still uses the pooled 8-channel encoding.
- The next meaningful ML step should come after stronger search/self-play targets, not before.
