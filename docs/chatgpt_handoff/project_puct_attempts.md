---
name: project_puct_attempts
description: PUCT search attempts — EVERYTHING FAILED, fundamental credit assignment problem unsolved
type: project
---

## PUCT Implementation History (2026-03-19) — STATUS: BROKEN

### The Fundamental Problem
Per-bird PUCT gives ALL birds IDENTICAL Q-values because the joint evaluation produces one value that gets backpropagated to every bird. There is ZERO per-bird signal. Every bird converges to "keep" (go straight).

Joint-action PUCT has the opposite problem: 81 actions, NN prior collapses to 100% on "all keep", only 1-3 visits for non-keep actions.

Neither approach produces a bot that moves short birds.

### What works (barely)
- Joint-action PUCT + additive log-prior + stochastic opponent + relative baseline + raw heuristic: the LONGEST bird moves and makes decent decisions. Beat Boss 2 once (26-25). But short birds never move.
- GCC pragmas (O3, AVX2, FMA): 25x inference speedup
- 8ch int4 QAT model trained, weights exported
- Danger score (wall proximity) and gravity danger added to heuristic
- Per-bird apple proximity weighted by 10/length
- Proportional terminal penalty (not flat 10000)

### What failed (ALL of these)
1. Multiplicative joint prior → collapses to 100% on one action
2. Per-bird independent PUCT → identical Q-values for all birds
3. Additive log-prior softmax → concentrated but better than multiplicative
4. Temperature 1.5, 4.0 on softmax → NN still says "keep" 91%
5. Prior floor 5% → not enough exploration
6. Uniform prior (remove NN) → same identical-Q problem in per-bird
7. 2-ply eval → too expensive, only 40 iterations
8. Deterministic opponent → wasted 99% of iterations re-computing same value
9. Stochastic opponent sampling → helps slightly but credit assignment unsolvable
10. Greedy apple-chase for short birds → code runs but doesn't help (short birds ≤4 should use greedy but PUCT search STILL runs and the compose step overrides)
11. Danger score / gravity danger in heuristic → values show up but search still picks keep
12. Alive bonus for short birds → too small to matter vs long bird's impact

### The Honest Truth
PUCT may be the wrong approach for this game at this iteration count. The old maximin exhaustively evaluated all 6561 pairs and WORKED (Silver 142). PUCT with 1000 iterations over 81 actions can't differentiate individual birds. Handed off to ChatGPT Pro for fresh perspective.

### Key CG Timing (full stack with pragmas + skipzero)
| Model | ms/eval | Evals/40ms |
|-------|---------|-----------|
| 8ch | 0.21 | 192 |
| 32ch | 2.30 | 17 |
| 48ch | 4.94 | 8 |
| 96ch | 19.23 | 2 |

### Files
- `c_bot/bot.c` — current bot with per-bird PUCT (broken)
- `docs/chatgpt_handoff/` — full handoff package for ChatGPT Pro
- `submission/c_bot_8ch_puct.c` — latest submission candidate (doesn't work)
