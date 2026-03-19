---
name: project_distill03_status
description: Distill03 state — models trained, PUCT search broken, handed off to ChatGPT
type: project
---

## Distill03 Status (2026-03-19 evening)

**Training: DONE.** 96ch/3L QAT trained to epoch 5 (80.2% policy, 0.556 val_corr). Grid training (8/12/16/24/32ch) was running on M4 Max but had a bug (chunk cycling epoch reset) — 8ch completed at 68.8% policy before the bug was discovered.

**C bot: EXISTS but broken.** `c_bot/bot.c` has full PUCT search implementation but the search doesn't work — short birds never move (credit assignment problem). See project_puct_attempts.md.

**GCC pragma optimization: CRITICAL.** Without `#pragma GCC optimize("O3,unroll-loops,fast-math")` and `#pragma GCC target("avx2,fma")`, the C conv is 25x slower. ALWAYS include these.

**Submission generator:** `tools/generate_c_submission.py` works. Can minify bot.c to fit 96ch weights (99.6% of 100K limit).

**Old working Rust submission:** `submission/flattened_main.rs` (80K chars) was at Silver 142 before PUCT experiments broke things. This uses maximin search + 12ch hybrid eval. Could be resubmitted as fallback.

**Leaderboard:** 1,495th/2,118 (Silver 1,081/1,096) — near last in Silver due to broken PUCT submissions.

**Next steps:** Get fresh perspective from ChatGPT Pro on the search architecture. The PUCT approach has a fundamental credit assignment problem that 12 hours of iteration hasn't solved.
