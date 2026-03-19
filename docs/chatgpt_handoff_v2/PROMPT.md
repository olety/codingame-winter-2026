# SNAKEBYTE Bot — Progress Update + Losing Game Analysis

## What Changed Since Last Review

We implemented all your recommendations except rollout deepening (which regressed in arena):

1. **alive_bonus removed** — eating now gives +120 body (was +0)
2. **PUCT replaced with maximin** — exhaustive 1-ply + selective deepening
3. **apple_race made competitive** — `(opp_dist - my_dist) / (my_dist + opp_dist + 1)` per apple
4. **CVaR scoring** — `(1-λ)*worst + λ*cvar3`, λ adaptive (0.05 ahead, 0.25 behind)
5. **apple_coverage** — distinct-apple greedy matching (EVAL_COVERAGE=40)
6. **EVAL_BREAKPOINT** — 9→25
7. **Better root ordering** — avg over 3 representative opp actions
8. **Fragile attack damped 75%** in all-len-3 openings
9. **danger/gravity penalties halved** from original
10. **Rollout deepening REVERTED** — non-adversarial rollouts gave misleading evals, hurt in arena

**Results:** 1,495th → 532nd global, Silver 1,081st → 106th. Local arena: v3b beats v2 31-17-12 over 60 seeds (+3.0 avg body diff).

## Current Eval Weights and Functions

```c
static float EVAL_BODY = 120.0f, EVAL_LOSS = 18.0f, EVAL_MOBILITY = 7.5f, EVAL_APPLE = 60.0f;
static float EVAL_STABILITY = 10.0f, EVAL_BREAKPOINT = 25.0f, EVAL_FRAGILE = 8.0f;
static float EVAL_COVERAGE = 40.0f;

// Danger: -100/-40/-10 (×1.5 for len≤3)
// Gravity: -15 unsupported (-40 for len≤3), -8 facing UP
```

## Current Search Architecture

```
choose_action_maximin():
  Phase 0: No scripting (SHORT_THRESHOLD=0, all birds searched)
  Phase 1: Enumerate my joint actions (3^N for N alive birds, max 81)
  Phase 2: Enumerate opp joint actions
  Phase 3: Order my actions by eval vs 3 representative opp actions
  Phase 4: Order opp actions worst-for-me first
  Phase 5: Maximin root pass with alpha-beta cutoffs
  Phase 6: Selective deepening — top 6 roots × all opp rescan × worst 8 deepened
           Deepening = mini-maximin 3×3 at next depth
           On T0: deepen ALL roots (time budget allows)
  Phase 7: CVaR ranking of deepened roots

Time budget: 950ms T0, 45ms later (CG limits: 1000/50)
```

## Three Losing Games — Detailed Analysis

**Common pattern across ALL three losses: the bot gets stuck outputting WAIT for 50-190 consecutive turns.**

### Game 1 (4v4, 167-turn map)
- T0-T12: Reasonable play, birds eat some apples
- T13: We're ahead 18-17 after opponent loses bird1
- T26: We lose a bird (beheaded), score drops but still ahead on losses
- T30-T167: **Bird5 stuck at (14,5) facing UP, apple_d=1, DANGER(wall=3).**
  - Only 3 actions: WAIT, LEFT, RIGHT
  - WAIT worst=-197.9, LEFT worst=-254.9, RIGHT worst=-huge (death)
  - Bot picks WAIT every turn for 137 turns straight
  - The apple is 1 tile away but requires a multi-step maneuver the 1-ply search can't see
  - Meanwhile bird0 at (7,8) also oscillates uselessly
  - End: lose 5-9 (body) but we lose because apples unreachable

### Game 2 (3v3, 73-turn map)
- T0-T9: Opening, opponent eats faster (opp bird3 gets to len 7)
- T10: Opponent loses bird4 (beheaded), we're ahead 12-10
- T17-T52: **Bird0 at (17,4) facing UP, apple_d=2. Outputs WAIT for 35 straight turns.**
  - Bird1 at (5,8) also stuck facing UP
  - The search correctly identifies WAIT as best worst-case
  - But the position is frozen — the opponent bird3 eats all remaining apples
- T53-T73: Terminal losing position, we eventually die

### Game 3 (3v3, 200-turn map)
- T0-T1: Reasonable opening
- T2: **Catastrophe — multiple birds die in collisions**
- T11-T200: **Bird5 alone at (14,5) facing UP, apple_d=1. Outputs WAIT for 189 consecutive turns.**
  - Exact same stuck pattern as Game 1
  - Only 3 actions, WAIT always wins on worst-case
  - Apple is right next to the wall — bird would need to go LEFT then DOWN to reach it
  - 1-ply search can't see the 2-step path

## The Root Cause: 1-Ply Horizon Lock

The maximin search evaluates each action by simulating 1 step forward, then evaluating the resulting position. When a bird is facing a wall with an apple to the side:

```
Wall: ################
Apple: A
Bird:  ^  (facing UP, 2 tiles from wall)

To eat the apple, bird needs: LEFT → DOWN (2 steps)
But 1-ply only sees:
  - KEEP (go up): approach wall, DANGER increases, but no collision yet
  - LEFT: move sideways, lose apple proximity in worst-case, opponent can punish
  - RIGHT: move away from apple entirely

Worst-case for LEFT is worse than worst-case for KEEP.
So bot picks KEEP. Next turn, same analysis. Forever.
```

The selective deepening (Phase 6) doesn't help because:
1. It deepens the TOP roots by worst-case — and WAIT is already the top root
2. The deepened evaluation still uses 1-ply mini-maximin at the child state
3. The deepening confirms that WAIT is "least bad" at every depth

## Proposed Fixes

### Fix 1: Anti-stall — force exploration after consecutive WAITs

```c
static int consecutive_waits = 0;
bool is_wait = true;
for (int i = 0; i < sr.action.n; i++)
    if (sr.action.cmd[i] != -1) { is_wait = false; break; }

if (is_wait) {
    consecutive_waits++;
    if (consecutive_waits >= 3) {
        // Force best non-WAIT action by rank_score
        for (int ri = 0; ri < nroots; ri++) {
            bool rw = true;
            for (int i = 0; i < my_acts[roots[ri].idx].n; i++)
                if (my_acts[roots[ri].idx].cmd[i] != -1) { rw = false; break; }
            if (!rw) { best_ri = ri; break; }
        }
        consecutive_waits = 0;
    }
} else {
    consecutive_waits = 0;
}
```

**Concern:** Band-aid. Forces action but doesn't know WHICH non-WAIT leads somewhere useful.

### Fix 2: Deeper search when few birds alive

When only 1-2 birds per side, branching is tiny (3×3=9 or 3×9=27 pairs). We can afford 2-3 ply:

```c
if (nmy <= 9 && nopp <= 9) {
    // Can afford much deeper search
    // 3-ply exhaustive = 27^3 = 19,683 evals = ~200ms
    // Fits in budget for most later turns
}
```

**Directly addresses Game 3** where bird5 alone vs 2 opp birds.

### Fix 3: Path-aware apple term

Add BFS distance to nearest apple instead of Manhattan distance, considering walls and current facing:

```c
// For each bird, BFS to find actual reachable apples
// Score by real path length, not Manhattan distance
// "Apple is 1 Manhattan but 3 BFS steps" gets scored correctly
```

**Concern:** BFS per bird per eval might be too expensive for every eval. Could use only for root ordering.

### Fix 4: Revisit rollout deepening with better policy

First attempt used simple per-bird greedy. A BFS-based policy would be much better:
- Each bird pathfinds to nearest reachable apple
- Respects walls and gravity
- Script both sides with this policy
- Evaluate after 3-4 step rollout

## Questions

1. **Is anti-stall (Fix 1) worth it, or too hacky?** Would immediately fix 190-turn WAIT spirals.

2. **For deeper search (Fix 2), iterative deepening vs fixed depth?**

3. **For path-aware apple (Fix 3), BFS per eval too expensive?** 4000 ops per eval × 6000 evals = 24M ops/turn.

4. **Should we revisit rollouts with BFS-based scripted policy?** The problem was the script, not the rollout idea.

5. **The opening still loses the apple race** (Game 1, T0-T6 opponent eats 1-2 more). Apple assignment heuristic for T0?

6. **43ms of unused budget when 1 bird left** (2ms used of 45ms). How to spend it?

7. **Any eval weight changes you'd recommend?** Current EVAL_APPLE=60 with competitive formula, EVAL_COVERAGE=40, EVAL_BREAKPOINT=25.

## Files Attached
- `bot.c` — current bot (the full code, 60K chars)
- `bot_old.c` — pre-eval-fix version for comparison
- All memory files for context
