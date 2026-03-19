---
name: project_distill03_status
description: C bot climbing Silver — v2 at 367th, v3b (CVaR+coverage) about to submit
type: project
---

## Distill03 Status (2026-03-19 night)

**C bot: WORKING and improving.** Went from 1,495th (broken PUCT) → 951st (maximin v1) → 790th/367th Silver (v2 eval fixes) in one session.

### Bot Architecture (v3b — current)
- **Search:** Exhaustive 1-ply maximin over all joint actions, alpha-beta cutoffs
- **Ordering:** My actions scored against 3 representative opp actions (default + 2 worst)
- **Deepening:** Top 6 actions × worst 8 opp × 3×3 mini-maximin
- **Ranking:** CVaR blend: `(1-λ)*worst + λ*cvar3`, λ adaptive (0.05 ahead, 0.25 behind)
- **No NN weights** — pure heuristic, 60K chars

### Eval Components
| Term | Weight | Description |
|------|--------|-------------|
| body | 120 | Raw body length diff (alive_bonus REMOVED) |
| loss | 18 | Loss diff |
| mobility | 7.5 | Legal commands per bird (unweighted) |
| apple | 60 | Competitive: (opp_dist-my_dist)/(total+1) per apple |
| coverage | 40 | Distinct-apple greedy matching |
| stability | 10 | Supported ±1 per bird (unweighted) |
| breakpoint | 25 | Count of len≥4 birds diff |
| fragile | 8 | Proximity to opp len≤3 birds (damped 75% in all-fragile) |
| danger | ~100 | Wall proximity (halved from original) |
| gravity | ~40 | Unsupported/facing-up penalties (halved) |

### Key Bugs Fixed (ChatGPT Pro diagnosed)
1. alive_bonus: `L + (10-L) = 10` made eating give +0 → REMOVED
2. PUCT credit assignment → replaced with maximin
3. Non-competitive apple_race → competitive Rust formula
4. Danger score drowning signal → halved
5. Length-weighted mobility/stability → unweighted

### What Was Tried and Reverted
- Scripted rollout deepening: non-adversarial, gave misleading evals → reverted to mini-maximin
- SHORT_THRESHOLD=4 scripting: bootstrap problem in opening → set to 0

### Training Assets (unused in current submission)
- 96ch/3L QAT: 80.2% policy, 0.556 val_corr
- 8ch int4 weights exported
- GCC pragmas mandatory for NN inference (25x speedup)

### Next Steps
- Submit v3b, track rank
- Consider training 32ch value-only net on search labels for leaf bonus
- Consider apple assignment-based ordering for opening
