---
name: leaderboard history
description: CodinGame WinterChallenge2026 leaderboard positions and submission tracking
type: project
---

## Leaderboard History

| Date | Rank | League | Bot | Notes |
|------|------|--------|-----|-------|
| ~2026-03-11 | 147th / 1,108 | Bronze | search-only 6/8/3/3 | First competitive config |
| ~2026-03-12 | 108th / 1,388 | Silver (108/763) | search-only 6/8/3/3 | League promotion to Silver |
| 2026-03-13 | 245th / 1,472 | Silver (245/890) | search-only 6/8/3/3 | Rank decay while training — field grew +84, dropped 137 places |
| 2026-03-13 | 187th / 1,478 | Silver (187/892) | hybrid-value-mid-12ch-distill01 | First hybrid submission settled — competition grew stronger too |
| 2026-03-18 | 479th / 2,046 | Silver (101/1,076) | hybrid-value-mid-12ch-distill01 | Rank decay — field grew +568, dropped 292 global but improved in-league (101 vs 187) |
| 2026-03-19 | 553rd / 2,108 | Silver (142/1,094) | hybrid-value-mid-12ch-distill01 | Continued decay — Gold league opened, boss upgraded |
| 2026-03-19 | 1,495th / 2,118 | Silver (1,081/1,096) | 8ch PUCT (broken) | Broken PUCT submission — collapsed prior, near last in league |
| 2026-03-19 | 951st / 2,130 | Silver (529/1,098) | C maximin v1 (old eval) | Maximin + alive_bonus fix, no NN |
| 2026-03-19 | 790th / 2,137 | Silver (367/1,102) | C maximin v2 (new eval) | Competitive apple_race, halved danger. +161 global, +162 in-league |
| 2026-03-19 | 532nd / 2,138 | Silver (106/1,099) | C maximin v3b | CVaR + coverage + breakpoint 25 + ordering. +258 global from v2 |

## Current Live Submission (v2 — about to be replaced by v3b)
- File: `submission/c_bot_maximin.c` (60K chars)
- Type: C bot, maximin search, pure heuristic (no NN weights)
- Search: exhaustive 1-ply + selective deepening (6/8/3/3)
- Eval: competitive apple_race, halved danger/gravity, no alive_bonus
- **v2 specific:** non-competitive apple_race, EVAL_BREAKPOINT=9, no coverage term

## V3b Changes (about to submit)
- CVaR scoring: `(1-λ)*worst + λ*cvar3` with adaptive λ (0.05 ahead, 0.25 behind)
- Apple coverage: distinct-apple greedy matching, EVAL_COVERAGE=40
- EVAL_BREAKPOINT: 9→25 (stronger 3→4 jump signal)
- Better root ordering: avg over 3 representative opp actions instead of 1
- Fragile attack damped 75% in all-len-3 openings
- Rollout deepening REVERTED (hurt performance — non-adversarial)
- Local arena: 31-17-12 vs v2 over 60 seeds, +3.0 avg body diff

**Why:** Tracking leaderboard positions helps gauge whether changes are improvements in the competitive context.
**How to apply:** After each submission, record the new rank once available. Compare against prior positions to validate pipeline effectiveness.
