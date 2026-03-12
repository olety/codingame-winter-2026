# Outer Loop Constitution

This document is the operating contract for the Prose-controlled outer loop.

## Objective

Find a candidate that beats the current incumbent under the authoritative arena + Java smoke regime, then promote it safely.

## Sacred Surfaces

The outer loop must not mutate:

- `rust/engine/**`
- `src/test/java/**`
- frozen arena suites under `config/arena/`
- `python/train/run_arena.py` acceptance semantics
- `python/train/java_smoke.py` correctness logic

## Mutable Islands

The outer loop may mutate, only through bounded candidate worktrees:

- `rust/bot/src/search.rs`
- `rust/bot/src/eval.rs`
- `rust/bot/src/features.rs`
- `rust/bot/src/hybrid.rs`
- `python/train/**`
- `automation/outerloop/**`
- candidate config JSONs

## Candidate Contract

Each candidate must produce:

- `genome.json`
- `stage0.json`
- `stage1.json`
- `stage2.json`
- optional `patch.diff`
- optional `model/`

All stage results must carry:

- `run_id`
- `candidate_id`
- `candidate_dir`
- `stage`
- `status`
- `artifact_hash`
- `behavior_hash`
- `genome_hash`
- `executor`

## Evaluation Ladder

1. **Stage 0**
   - materialize candidate
   - optional bounded patch
   - build sanity
   - optional hybrid training

2. **Stage 1**
   - smoke screening only
   - never authoritative
   - never promotable

3. **Stage 2**
   - heldout vs incumbent
   - shadow vs anchor
   - Java smoke on exact built artifact
   - later-turn timing gate

## Promotion Rule

Only a stage-2 candidate with `status=accepted` may promote.

Promotion must:

- rotate current `submission_current.json` into `incumbent_current.json`
- install the winning `candidate_config.json` as `submission_current.json`
- regenerate `submission/flattened_main.rs`
- archive the winning config under `rust/bot/configs/archive/`

## Stop Conditions

The loop stops when:

- a promotable winner is found and promoted
- the configured generation budget is exhausted
- a sacred-surface violation is detected
- the operator interrupts the run
