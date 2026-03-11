# 2026-03-12 Live Results and Architecture Storyline

This note is a lightweight narrative memory surface for future writeups.

Use it when we want to answer:

- what the current bot architecture actually is
- which agent/config is currently live
- how the leaderboard moved over time
- what the key turning points were

## Current architecture

The current stack is:

- Java-parity Rust engine for rules, gravity, collisions, mapgen, and exact forward simulation
- Rust contest bot with embedded build-time submission config
- heuristic search bot with exhaustive root evaluation plus selective depth-2 refinement
- release-mode Rust arena harness for heldout/shadow comparisons
- Java smoke canary for exact built-artifact verification against the real referee loop
- Rust-seed self-play/export pipeline and a paused Python value-training path
- generated single-file submission artifact at `submission/flattened_main.rs`

The current search philosophy is still search-first, not ML-first:

- exact simulator
- robust arena evaluation
- search config tuning
- only then consider richer self-play / ML work

## Current agent lineup

### Live submission

File:

- `rust/bot/configs/submission_current.json`

Current promoted search shape:

- `deepen_top_my = 6`
- `deepen_top_opp = 8`
- `deepen_child_my = 3`
- `deepen_child_opp = 3`
- `later_turn_ms = 40`

Interpretation:

- broader root and opponent coverage
- shallower child follow-up search
- timing-safe under the current later-turn gate

### Prior incumbent

File:

- `rust/bot/configs/incumbent_current.json`

Preserved prior baseline:

- `deepen_top_my = 6`
- `deepen_top_opp = 6`
- `deepen_child_my = 4`
- `deepen_child_opp = 4`
- `later_turn_ms = 40`

Interpretation:

- more balanced root/child allocation
- was the first stable baseline that looked genuinely competitive

### Weak anchor

File:

- `rust/bot/configs/anchor_root_only.json`

Shape:

- all deepening disabled
- `extra_nodes_after_root = 0`

Purpose:

- deliberately weaker comparison point
- useful for catching regressions that still beat nothing

## Key offline turning point

The first apparently strong `tmy4/topp4/cmy5/copp5` family did **not** survive authoritative reruns.

The first true promotion came from:

- `sweep_tmy6_topp8_cmy3_copp3_lat40`

Authoritative acceptance evidence:

- heldout body diff `+0.08984375`
- heldout win margin `0.0`
- shadow body diff `+9.33203125`
- later-turn `p99 = 41 ms`
- Java smoke passed

Important nearby failures:

- `tmy6/topp8/cmy3/copp4` lost heldout and failed timing
- `tmy6/topp8/cmy5/copp5` had stronger raw heldout body diff but failed timing badly

The lesson from that promotion:

- broader top-level coverage beat deeper child search
- timing gate mattered more than raw heldout body diff alone

## Live leaderboard history

Known checkpoints:

| Date | Config | Rank | League | Score | Notes |
| --- | --- | --- | --- | --- | --- |
| 2026-03-11 | Pre-promotion baseline (`6/6/4/4`) | `168 / 1108` | Bronze | not recorded | First solid live baseline |
| 2026-03-12 | Promoted breadth-heavy winner (`6/8/3/3`) | `147 / 1108` | Bronze | `29.57` | First live post-promotion checkpoint |

Current comparison delta:

- rank improvement: `+21` places
- score baseline for the current promoted bot: `29.57`

## Blog-post fuel

If we write about this later, the interesting story is probably:

1. build exact local truth first
2. stop trusting pretty sweep winners until heldout + timing + smoke all agree
3. discover that the first flashy winner was fake
4. get the first true promotion from a modest but robust breadth-heavy config
5. watch the live leaderboard move in the same direction as the local evidence

Useful headline themes:

- “Why the first winning sweep result was wrong”
- “Breadth beat depth in our first real Winter Challenge promotion”
- “From exact simulator parity to the first live rank bump”

## What to append next

Add a new row whenever any of these happen:

- a new CodinGame live score/rank checkpoint
- a new promoted submission config
- a materially different architecture phase, like opponent-pool self-play or policy/value integration

Keep this file narrative-friendly and compact. The deeper implementation details should stay in the other memory docs.
