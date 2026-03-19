---
name: project_puct_attempts
description: PUCT search — ABANDONED. Replaced with maximin. ChatGPT Pro diagnosed the root causes.
type: project
---

## PUCT — ABANDONED (2026-03-19)

### Why PUCT Failed (ChatGPT Pro diagnosis)
1. **Not actually tree search** — root-only 1-ply bandit, not MCTS
2. **Per-bird identical stats by construction** — same prior + same scalar backup = identical Q forever
3. **DUCT at wrong level** — should be between two players, not between birds on same team
4. **alive_bonus flattened short-bird growth** — `L + (10-L) = 10` made eating give +0

### What Replaced It
Maximin search (ported from working Rust bot):
- Exhaustive 1-ply over all my × all opp joint actions
- Alpha-beta cutoffs with heuristic-ordered actions
- Selective deepening: top 6 actions × worst 8 opp × 3×3 child branching
- SHORT_THRESHOLD=0 (search all birds, don't script any)

### Result
- Old eval maximin: 951st/2,130 (Silver 529/1,098) — up from 1,495th
- New eval (competitive apple_race, halved danger): 15-4-1 vs old in local arena

### Key CG Timing (full stack with pragmas + skipzero)
| Model | ms/eval | Evals/40ms |
|-------|---------|-----------|
| 8ch | 0.21 | 192 |
| 32ch | 2.30 | 17 |
| 48ch | 4.94 | 8 |
| 96ch | 19.23 | 2 |

### Files
- `c_bot/bot.c` — current bot with maximin search (WORKING)
- `c_bot/bot_old.c` — pre-eval-fix version for local arena comparison
- `submission/c_bot_maximin.c` — latest submission
- `submission/c_bot_maximin_debug.c` — debug version with verbose eval breakdown
