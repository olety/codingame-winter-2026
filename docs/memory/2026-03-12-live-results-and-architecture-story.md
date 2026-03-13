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
- **hybrid value head**: distilled 12ch/3L CNN student (from 128ch/8-block SE teacher), value-only leaf integration (leaf_mix=0.08, prior_mix=0.0)
- release-mode Rust arena harness for heldout/shadow comparisons
- Java smoke canary for exact built-artifact verification against the real referee loop
- Modal-backed distillation pipeline: shared self-play → teacher training → soft target generation → parallel student distillation → screening → authoritative evaluation
- generated single-file submission artifact at `submission/flattened_main.rs` with embedded hybrid weights

The search philosophy is now **search + value hybrid**:

- exact simulator
- robust arena evaluation
- search config tuning (6/8/3/3 breadth-heavy)
- value head provides leaf evaluation signal that search alone cannot discover
- policy priors confirmed harmful at current model quality — value-only integration is the path

## Current agent lineup

### Live submission

File:

- `rust/bot/configs/submission_current.json`

Current promoted config: **hybrid-value-mid-12ch-distill01**

Search shape (unchanged):
- `deepen_top_my = 6`, `deepen_top_opp = 8`, `deepen_child_my = 3`, `deepen_child_opp = 3`, `later_turn_ms = 40`

Hybrid config:
- `prior_mix = 0.0` (policy priors disabled)
- `leaf_mix = 0.08` (8% value head blending at leaf nodes)
- `value_scale = 48.0`
- Weights: `rust/bot/weights/distill_c2_12ch_3L.json` (12 channels, 3 conv layers, distilled from 128ch/8-block SE teacher)

Authoritative result vs search-only incumbent:
- heldout body diff: **+0.94** (512 matches)
- shadow body diff: **+9.56** (512 matches)
- p99 timing: **40ms** (within 45ms gate)

### Prior incumbent (now rotated)

File:

- `rust/bot/configs/incumbent_current.json`

Now contains the search-only `6/8/3/3` that was previously the submission. The old `6/6/4/4` config is archived at `rust/bot/configs/archive/search_only_6833_silver108.json`.

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
| 2026-03-13 | Same (`6/8/3/3`) | `108 / 1388` | Silver | — | League promotion to Silver (108th / 763 Silver) |
| 2026-03-13 | Same (`6/8/3/3`) — rank decay | `245 / 1472` | Silver (245/890) | — | Field grew +84 players, dropped 137 places while training pipeline ran |
| 2026-03-13 | **Hybrid `value-mid-12ch-distill01`** | 187 / 1,478 | Silver (187/892) | — | First hybrid submission — settled at 187th as competition strengthened |

Current comparison delta:

- rank trajectory: `168 → 108 → 245 (decay) → 187 (hybrid)`
- league: Bronze → Silver (maintained)
- first hybrid bot promoted: +0.94 heldout body diff vs search-only incumbent over 512 authoritative matches
- competition grew stronger — 187th/892 Silver vs previous 245th/890 Silver means field got tougher while we improved

## Blog-post fuel

Key story beats:

1. Build exact local truth first (Java-parity simulator)
2. Stop trusting pretty sweep winners until heldout + timing + smoke all agree
3. First true promotion from modest but robust breadth-heavy config (6/8/3/3)
4. Watch live leaderboard move in same direction as local evidence (168 → 108, Bronze → Silver)
5. **Build distillation pipeline**: teacher-student knowledge distillation to add ML signal to search
6. **Discover value-only integration**: policy priors hurt at 61% accuracy, but value head (0.52 correlation) helps at very low mixing
7. **First hybrid promotion**: 12ch/3L distilled student + leaf_mix=0.08 beats the search-only incumbent
8. Extend submission generator to embed NN weights as static Rust arrays
9. **Submission compaction**: from 236K to 89K chars via indent reduction and comment stripping
10. **Unicode encoding trick**: ~2.6× more neural net per character — enabling 3× larger models in the same 100K limit

Useful headline themes:

- “From search-only to hybrid: when 8% of a tiny neural net beats pure search”
- “Why policy priors failed but value heads worked”
- “Distilling 2.5M parameters into a 138KB model that runs in 2ms”
- “Unicode encoding trick: 2.6× more neural net per character”
- “How we fit a 24-channel CNN into a 100K character source file”

## Architecture evolution (2026-03-13)

Key changes committed in the distillation pipeline batch:

1. **Rust inference 2-3x speedup**: Flattened `Vec<Vec<Vec<f32>>>` to flat `Vec<f32>` with stride indexing in `hybrid.rs`. Eliminates triple-pointer indirection. 8ch inference estimated ~0.9ms instead of ~2.7ms.
2. **3rd conv layer support**: Optional `conv3` in `TinyHybridWeights` (backward compatible). Student receptive field 5x5 → 7x7, covering ~30% of the 23-tile short dimension. Only +0.3ms overhead.
3. **1x1 kernel support**: Kernel offset now uses `kernel_size/2` instead of hardcoded `1`. Enables bottleneck architectures later.
4. **Schema v2**: Rust accepts both v1 (2-layer) and v2 (3-layer) weight files.
5. **Modal Volume migration**: Datasets transferred via `modal.Volume("snakebot-datasets")` instead of gzip+base64 JSON blobs. Fixes aiohttp/SSL/broken pipe errors on 40MB-2.3GB transfers.
6. **Retry logic**: All `.remote()` calls wrapped with exponential backoff + jitter (3 retries, catching ConnectionError/OSError/TimeoutError).
7. **Shared dataset generation**: `build_shared_dataset()` generates one big self-play dataset (500+ seeds) reused by all parallel candidates. Eliminates 8x redundant self-play from same config.
8. **Teacher-student distillation pipeline**: TeacherHybridNet (128ch, 8 SE-res blocks, ~2-3M params) → soft target generation → KL+MSE distillation training for student. Loss: `T²·KL(student/T, teacher/T) + 1.5·MSE(value) + α·CE(hard_targets)`.
9. **CodinGame timing probe**: Embedded timing loop in submission to measure actual CNN inference on CG servers. CG hardware is ~8× faster than local Mac. Results: 12ch/3L unoptimized 4.74ms, 24ch/3L optimized 2.28ms (f32) / 2.42ms (f16), 28ch/3L 4.03ms, 32ch/3L 3.89ms. All well within 45ms gate — timing is NOT the constraint, 100K char limit is.
10. **Submission compaction**: Stripped comments/blank lines + reduced indentation (4-space → 1-space per nesting level). 236K → 89K chars. Fits 100K limit.
11. **Unicode weight encoding**: Replaced base64 (5.33 chars/f32) with BMP 2-byte encoding (~2.0 chars/f32). ~2.6× density improvement. 89K → 72K chars for 12ch.
12. **Conv optimization**: Loop reorder (ic outside y/x) + interior/edge split + manual 3×3 unroll. CG timing: 24ch/3L at 2.28ms/call — timing is not a constraint.
13. **f16 quantization**: f32→f16 halves weight bytes. Combined with Unicode BMP: 1 char per weight (5.3× denser than base64). Enables 32ch/3L (91K chars) within 100K limit.
14. **CodinGame has numpy 1.23.2**: Python 3.11, scipy 1.9.3 available. 64×64 matmul at 0.20ms/call. Enables Python-based NN bot with ~5K code + ~95K weight budget → 96ch/3L model.
15. **Strategic pivot to dual-path**: Rust (search + 32ch NN) AND Python (pure 96ch NN + numpy). Both distilled from a much larger 512ch/16-block teacher trained on 20K seeds (4.6M positions) with Muon optimizer. Self-play data generated free on local compute fleet (Contabo VPS + Mac Mini M4).

## What to append next

Add a new row whenever any of these happen:

- a new CodinGame live score/rank checkpoint
- a new promoted submission config
- a materially different architecture phase, like opponent-pool self-play or policy/value integration

Keep this file narrative-friendly and compact. The deeper implementation details should stay in the other memory docs.
