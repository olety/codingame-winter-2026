---
name: codingame hardware timing
description: MEASURED CNN inference timing on CodinGame servers. NEVER GUESS THESE NUMBERS.
type: project
---

## CodinGame Hardware Timing

### C bot int4 conv WITH pragmas (2026-03-19) — USE THESE

```c
#pragma GCC optimize("O3,unroll-loops,fast-math")
#pragma GCC target("avx2,fma")
```
Plus `__restrict__` on pointers, hoisted dequant weights, row-pointer inner loop.

| Model | Per-call (ms) | Iters | Fits 40ms turn? | Evals in 40ms |
|-------|---------------|-------|-----------------|---------------|
| 32ch/3L | **1.79** | 10 | YES | ~22 |
| 48ch/3L | **3.83** | 5 | YES | ~10 |
| 64ch/3L | **6.44** | 3 | YES | ~6 |
| 96ch/3L | **13.84** | 3 | YES | ~2-3 |

### C bot int4 conv WITHOUT pragmas (2026-03-19) — DO NOT USE

| Model | Per-call (ms) | Notes |
|-------|---------------|-------|
| 32ch/3L | 45.80 | GCC default = no vectorization |
| 64ch/3L | 183.96 | 29x slower than with pragmas |

**The pragmas are MANDATORY.** Without them, GCC doesn't vectorize and everything is 25-30x slower.

### Key optimizations that matter
1. `#pragma GCC optimize("O3,unroll-loops,fast-math")` — forces O3 optimization
2. `#pragma GCC target("avx2,fma")` — enables SIMD instructions
3. `__restrict__` on pointer params — no-alias guarantee for vectorizer
4. Hoisted weight dequant (9 floats before x-loop) — keeps inner loop clean for vectorizer
5. Row pointers (r0, r1, r2, dst) — contiguous memory access pattern

### CG time limits
- First turn: 1000ms (effective ~850ms) → ~60 evals at 96ch
- Subsequent turns: 50ms (effective ~40ms) → ~2-3 evals at 96ch

### PUCT viability
At 96ch/13.84ms: 2-3 NN evals per turn. Enough for policy prior at root + 1-2 leaf evals.
At 48ch/3.83ms: ~10 NN evals per turn. Comfortable for PUCT with NN leaf evaluation.

**How to apply:** ALWAYS include the two pragma lines at the top of any C submission. Timing probe code at `submission/timing_96ch_opt.c`.
