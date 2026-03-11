# 2026-03-11 Engine, Search, and Evaluation Harness

## Engine semantics

The Rust engine now keeps Java-parity natural game end separate from contest terminal logic.

- `GameState::is_game_over()` remains the Java-parity natural end check.
- `GameState::is_terminal(max_turns)` adds the contest turn cap.
- `GameState::final_result(max_turns)` exposes explicit body scores, final scores, losses, body diff, loss diff, winner, and terminal reason.

This avoids muddying the parity tools while still giving search, arena, and self-play the right contest semantics.

## Parity status

The important parity fixes already in place are:

- Java RNG bounded-range parity in `java_random.rs`
- per-turn bird direction reset parity in `state.rs`

The strongest recent parity check that passed was:

```bash
cargo run -q -p snakebot-engine --bin java_diff -- --init-source rust --seeds 50 --turns 20
```

## Search behavior

The Rust search path now has:

- live budgets of `850 ms` on turn `0` and `40 ms` afterwards
- deterministic offline budget mode: exhaustive root first, then `extra_nodes_after_root`
- heuristic ordering for friendly actions and opponent replies
- incumbent-based early cutoff inside the opponent loop
- depth-2 top-k follow-up search after the root pass

The current implementation intentionally does **not** include a transposition table yet. The current order of work is:

1. ordering
2. cutoff
3. depth-2 top-k
4. TT only if profiling shows it is worth the extra complexity

## Arena harness

There is now a release-mode Rust arena binary:

- `rust/bot/src/bin/arena.rs`

It:

- runs two configs against each other in-process
- swaps seats automatically
- parallelizes across seeds
- reports average body diff, W/D/L, tiebreak win rate, average turns, node counts, and move-time percentiles

Frozen committed suites:

- `config/arena/smoke_v1.txt`
- `config/arena/heldout_v1.txt`
- `config/arena/shadow_v1.txt`

## Live-path canary

There is also now a small Java-referee smoke canary:

- Java CLI: `src/test/java/com/codingame/game/RunnerCli.java`
- Python wrapper: `python/train/java_smoke.py`

The canary runs the real compiled Rust bot through the actual referee runner and can force hard failure on reconciliation fallback via:

```bash
SNAKEBOT_STRICT_RECONCILE=1
```

## Recent runtime checks

Short checks that passed after the arena/smoke work landed:

- tiny 2-seed release-mode arena run: same-config draw, `p99` move time around `15-17 ms`
- Java smoke on boss + mirror matches: passed with zero reported runner error counts

## Still intentionally basic

- No TT yet
- No policy prior or network-guided search yet
- Current arena acceptance logic is still simple body-diff/WDL based, not a full Elo framework
