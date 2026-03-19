# SNAKEBYTE Bot — PUCT Search Crisis: Need Fresh Eyes

## What This Is

I'm building a bot for CodinGame's SNAKEBYTE competition (Winter Challenge 2026). It's a 2-player simultaneous-move snake game with gravity. Each player controls 2-4 snakes ("birds") on a 24x44 grid. Score = total body length of alive birds at turn 200.

I've been working with Claude Code (Opus 4.6) for ~12 hours straight trying to implement PUCT search. We've gone through dozens of iterations and **nothing works properly**. The bot either:
1. Outputs WAIT (all birds go straight) every turn and dies
2. Only moves the longest bird while short birds do nothing
3. All birds pick the exact same action (credit assignment failure)

**I need you to look at this with completely fresh eyes and tell me what's fundamentally wrong.**

## The Core Problem

### Per-Bird Independent PUCT (Current Approach)
Each bird independently selects its action via PUCT (3 options: keep/left/right). The joint action is composed from all birds' independent selections. Evaluation simulates all birds together + stochastic opponent.

**THE BUG:** All birds receive IDENTICAL Q-values for all their actions. Look at any debug output:
```
b0(0): -1:N=1022,Q=-565.1,P=0.33  2:N=1,Q=-1982.9,P=0.33  0:N=1,Q=-2267.5,P=0.33
b1(1): -1:N=1022,Q=-565.1,P=0.33  3:N=1,Q=-1982.9,P=0.33  1:N=1,Q=-2267.5,P=0.33
b2(2): -1:N=1022,Q=-565.1,P=0.33  3:N=1,Q=-1982.9,P=0.33  1:N=1,Q=-2267.5,P=0.33
```
Every bird has the same N, same Q, same everything. They all converge to "keep" (-1) because keep always has the best Q (least negative).

**WHY:** When all birds select independently via PUCT, they all pick the same action (because they start with same prior and same initial Q). The evaluation then produces one joint value that gets backpropagated to ALL birds identically. There's zero signal about what any INDIVIDUAL bird should do.

### Previous Approach: Joint-Action PUCT
PUCT over all 81 joint actions (3^4 for 4 birds). Had the OPPOSITE problem: the NN policy prior collapsed to 100% on "all keep" because the per-bird softmax (60% keep) gets multiplied across birds (0.6^4 ≈ 13% but after softmax → 99.9%). Even with fixes (additive log-prior, temperature, floor), the search only explored the top action and everything else got 1-3 visits.

**When joint-action PUCT "worked" (barely beat Boss 2 once):** The longest bird moved but short birds stayed on WAIT for 100+ turns because the heuristic difference between "short bird turns" vs "short bird keeps" was tiny (~3 points) compared to the long bird's impact (~100+ points).

## What's Been Tried (All Failed)

1. **Multiplicative joint prior** → collapsed to 100% on one action
2. **Additive log-prior** → better distribution but still concentrated
3. **Per-bird independent PUCT** → all birds get identical Q-values
4. **Softmax temperature (1.5, 4.0)** → helped slightly but NN still says "keep" 91%+
5. **Prior floor (5%)** → not enough exploration
6. **Dirichlet noise** → broken implementation
7. **Uniform prior (remove NN)** → same identical-Q problem
8. **2-ply evaluation** → too expensive, only 40 iterations
9. **Stochastic opponent sampling** → helps differentiation slightly but not enough
10. **Relative baseline scoring** → fixed the "all actions equally bad" problem
11. **Greedy apple-chase for short birds** → short birds still output WAIT (the greedy code runs but PUCT overrides it?? unclear)
12. **Danger score (wall proximity)** → penalty values are there but search still picks keep
13. **Gravity danger** → same

## The Actual Code

The bot is in `bot.c` (attached). Key sections:
- `evaluate()` — heuristic eval with body diff, mobility, apple race, danger, gravity
- `choose_action_puct()` — the per-bird PUCT search (bottom of file)
- `ordered_cmds()` — legal commands per bird
- `sim_state()` / `step()` — game simulation

## Constraints
- C language, single file, <100K chars total (code + weights)
- 40ms per turn on CG servers (850ms first turn)
- 8ch CNN provides policy/value but the policy is useless (always says "keep")
- ~1000-5000 PUCT iterations per turn (2 evals per iteration)

## Game Mechanics That Matter
- **Gravity**: birds fall if unsupported. Moving up fights gravity.
- **Scoring**: total body length of alive birds. Dead birds = 0.
- **Length 3 birds are fragile**: any collision = instant death. Length 4+ survives beheading.
- **Eating an apple**: grow by 1. Going 3→4 is the biggest qualitative jump.
- **Simultaneous moves**: both players act at the same time.

## What I Need

1. **Why does per-bird PUCT give identical Q-values?** The evaluation is stochastic (random opponent each iteration), so different iterations should give different values. But the SAME value gets backpropagated to ALL birds because they all participated in the same simulation. Is there a way to decompose the joint evaluation into per-bird contributions?

2. **Is there a fundamentally different search approach** that works for simultaneous-move games with multiple agents on the same team? The Lanctot et al. paper says Decoupled UCT works best, but our implementation has the identical-Q problem.

3. **Should I just give up on search for short birds** and use a simple greedy rule (move toward nearest apple, avoid walls)? The search adds nothing for them.

4. **The old maximin search worked** (Silver league, rank 142). It evaluated ALL 6561 joint action pairs exhaustively at 1-ply. Should I just go back to that with the NN improvements (pragma optimizations, better heuristic)?

## Files Attached
- `bot.c` — the full bot code
- `MEMORY.md` — index of all memory files
- `project_puct_attempts.md` — detailed history of what was tried
- `project_cg_timing.md` — CG inference timing data
- `project_distill03_status.md` — training status
- All other memory files for context

## Claude's Assessment

I think PUCT is the wrong approach for this game at this time budget. The fundamental issue is that with 81 joint actions and only ~1000 iterations, there aren't enough samples to differentiate individual birds' contributions. The old maximin exhaustively evaluated everything and just... worked. But the user wants PUCT and I've been going in circles for 12 hours.

Please be brutally honest about what's wrong and what the right path forward is.
