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

Screenshot: `screenshot_2026-03-13_rank245_pre_hybrid.png` (before hybrid submission)

## Current Live Submission
- Config: `rust/bot/configs/submission_current.json`
- Name: hybrid-value-mid-12ch-distill01
- Type: search 6/8/3/3 + value-only hybrid (leaf_mix=0.08, 12ch/3L distilled)
- Flattened: `submission/flattened_main.rs` (7619 lines, weights embedded)

**Why:** Tracking leaderboard positions helps gauge whether changes are improvements in the competitive context.
**How to apply:** After each submission, record the new rank once available. Compare against prior positions to validate pipeline effectiveness.
