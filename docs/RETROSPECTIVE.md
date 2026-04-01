# WinterChallenge2026 Retrospective

**CodinGame Winter Challenge 2026 — "Exotec"**
Author: Oleksii Kyrylchuk
Final rank: **Silver ~73rd-124th** (out of ~2,161 global, ~1,107 in Silver)
Bot: Pure C maximin heuristic — no neural network

---

## Timeline

| Date | Event | Rank |
|------|-------|------|
| ~Mar 11 | First Rust search bot (6/8/3/3) | Bronze 147th/1,108 |
| Mar 12 | Promoted to Silver | Silver 108th/1,388 |
| Mar 13 | First hybrid NN bot (12ch value-only) | Silver 187th/1,478 |
| Mar 14 | Muon vs AdamW A/B test on Modal | — |
| Mar 17 | 128ch teacher trained on 36.8M positions | — |
| Mar 19 | Distill02 students trained, arena tested | — |
| Mar 19 | PUCT attempt — collapsed to near-last | Silver 1,081st/1,096 |
| Mar 19 | Pivot to C maximin (no NN) | Silver 529th |
| Mar 19 | Improved eval (competitive apple_race) | Silver 367th |
| Mar 19 | v3b: CVaR + coverage + breakpoint | Silver **106th** |
| Mar 20 | Peak rank after settling | Silver **73rd** |
| Mar 20 | v6: CVaR fix + team stall hook | Silver 124th (field tightened) |

## The Big Picture

I spent roughly **3 weeks** and **~$310** on this challenge. About 2 of those weeks and nearly all the money went into building an ML pipeline — self-play data generation, teacher training, knowledge distillation, inference optimization — that ultimately **did not ship in the final bot**.

The final submission was a pure C heuristic bot with maximin search. No neural network weights. No learned evaluation. Just hand-tuned terms and 1-ply exhaustive search with selective deepening.

## What Shipped (and worked)

### The C Maximin Bot

The final bot (`submission/c_bot_maximin.c`, ~60K chars) used:

- **Exhaustive 1-ply maximin search** over all my-actions × all opponent-actions
- **Alpha-beta cutoffs** with heuristic action ordering
- **Selective deepening**: top 6 actions × worst 8 opponent actions × 3×3 child branching
- **CVaR scoring**: `(1-lambda)*worst + lambda*cvar3` with adaptive lambda (0.05 when ahead, 0.25 when behind)
- **Handcrafted eval** with 10 terms: body length, losses, mobility, competitive apple distance, apple coverage, stability, breakpoint (len>=4 count), fragile bird proximity, wall danger, gravity

Key eval innovations:
- **Competitive apple_race**: `(opp_dist - my_dist) / (total + 1)` per apple, not just Manhattan distance
- **Coverage term**: distinct-apple greedy matching (weight=40)
- **Breakpoint at 25**: strong signal for the 3->4 length transition
- **Halved danger/gravity**: original weights were too conservative

### What Made It Good

1. **Fast iteration** — changing a heuristic weight and testing took minutes, not hours
2. **Domain knowledge** — understanding *why* birds die led to better eval terms than any learned function
3. **CVaR** — scoring by worst-case + tail average (not just worst case) was a breakthrough
4. **Time management** — 950ms first turn, 45ms subsequent, well within CG limits

## What Didn't Ship (the ML detour)

### The Full ML Pipeline

Built a complete teacher-student distillation pipeline:

1. **Distributed self-play** across 4 machines (M4 Max, Mac Mini M4, Contabo VPS, Arch desktop)
   - 164K+ seeds, 36.8M unique training positions
   - Bitpacked binary format (2,528 bytes/row), sharded (93 shards, 93GB on R2)
2. **128ch/8-block SE-ResNet teacher** trained with Muon optimizer
   - val_corr=0.6728, policy_acc=90.32%
   - Cost: ~$28 on Modal H100
3. **Student distillation** (12ch, 24ch, 32ch, 48ch, 64ch variants)
   - Soft target KD with T=2.0, alpha=0.3
   - Best 32ch student: val_corr=0.6245, policy_acc=78.38%
4. **Submission encoding** — f16 Unicode BMP encoding (1 char/weight, 5.3x denser than base64)
5. **CodinGame timing probes** — measured actual CNN inference on CG servers

### Why ML Didn't Win

1. **Policy priors hurt at <85% accuracy.** Even at prior_mix=0.03, the 78% policy prior made search worse. All prior-based candidates lost arena matches.

2. **Value-only integration was marginal.** The best hybrid (leaf_mix=0.08, prior_mix=0.0) achieved +0.94 body diff over search-only — real but small. The competition was too tight for that edge to matter much.

3. **PUCT was fundamentally broken.** Our "PUCT" implementation was actually a root-only 1-ply bandit, not tree search. Per-bird stats were identical by construction. ChatGPT Pro diagnosed the root causes after we submitted it and it collapsed to near-last. This was the moment that killed the ML path.

4. **Search depth matters more than eval quality** at this competition level. The C bot's 1-ply maximin with good heuristics beat 32ch NN + search hybrid. A 2-ply search with a mediocre eval would likely beat both.

5. **Pure-NN Python bot went 0-16** against any search bot. 80% policy accuracy without search is worthless.

## Money Spent

| Item | Cost | Outcome |
|------|------|---------|
| Modal H100 teacher training | ~$28 | Good teacher, students unused in final bot |
| Modal A100 sweeps + distillation | ~$3 | Students trained but not competitive enough |
| Modal misc (A/B test, soft targets) | ~$5 | Validated Muon optimizer — useful learning |
| Modal hidden memory charges | ~$119 | Surprise bill from 192GB containers + mmap |
| Modal old workspace overrun | ~$143 | From previous workspace, not this project |
| Vast.ai sweeps | ~$15 | Failed runs, instances left running after crashes |
| Self-play compute (local) | $0 | Weeks of CPU time on 4 machines |
| **Total out-of-pocket** | **~$310** | |

The $119 Modal memory surprise was particularly painful. Memory-mapping a 93GB dataset in Modal containers costs $0.74/hr *per container* on top of GPU costs. Four parallel H100 jobs ran up the memory bill faster than the GPU bill.

## Honest Reflections

### The data generation was mostly wasted

We generated 36.8M training positions across 4 machines over multiple days. The self-play data was high-quality — the bitpacked format was elegant, the pipeline was robust, the incremental append workflow worked well. But the data trained a teacher whose students couldn't meaningfully improve on hand-tuned heuristics.

The ROI on the data generation was approximately zero for the competition. The infrastructure and tooling knowledge was valuable for learning, but you could argue the same learning cost could have been ~$20 instead of ~$300 if we'd validated the integration path first.

### Should have validated the hybrid path before scaling data

The right sequence was:
1. Build a tiny hybrid with 2K seeds (did this — distill01)
2. Submit it and see if it's competitive (did this — marginal improvement)
3. **Stop here and ask: is 10x more data going to 10x the improvement?**

Instead, we scaled to 164K seeds and a full distributed pipeline before confirming the answer was "probably not." The fundamental issue — search depth, not eval quality — wouldn't be solved by more data.

### The PUCT disaster was a turning point

Submitting a broken PUCT bot that collapsed from Silver 106th to 1,081st was embarrassing but clarifying. It forced the pivot to pure C heuristics, which was the right call all along for this competition format.

### C heuristics iterate faster than ML

Changing an eval weight in C and testing locally takes 2 minutes. Training a new student, exporting weights, embedding them, and running an arena takes 2 hours minimum. At competition speed, fast iteration dominates.

### ChatGPT Pro was genuinely useful

When we were stuck on why PUCT failed, ChatGPT Pro (o1) diagnosed the root causes in one conversation: not real tree search, per-bird stats identical by construction, DUCT applied at wrong level, alive_bonus flattening growth. This saved hours of debugging.

## Technical Learnings Worth Preserving

### ML / Training

- **Muon optimizer** beats AdamW by +10.9% val_corr, +10.3% policy_acc on CNN training. Use it.
- **torch.compile mode="reduce-overhead"** gives same speed as max-autotune but compiles instantly (vs 80+ min). Never use max-autotune.
- **channels_last conflicts with Muon** — Muon's `.view()` fails on non-contiguous tensors. Would need a `.reshape()` fork.
- **Mmap works for training even when dataset >> RAM.** 93GB on 64GB machine works fine at 75ms/step. Don't reflexively subset.
- **Early stopping is essential** — 128ch teacher saturated at epoch 3 of 19. Without patience=3, would have burned 5x the compute.
- **torch.compile checkpoints** have `_orig_mod.` prefix on keys. Strip when loading into uncompiled model.
- **Soft target distillation**: T=2.0, alpha=0.3 consistently wins across model sizes.

### Infrastructure

- **Modal hidden memory charges** — memory-mapping large datasets in containers costs $0.008/GiB/hr. 93GB = $0.74/hr per container. Always check memory= in job config.
- **R2 CloudBucketMount is slow for random access** — each page fault is a network round-trip. Preload into RAM for shuffled training.
- **Vast.ai CLI output is not JSON** — it's `Started. {'success': True, ...}`. Must use `ast.literal_eval`.
- **Vast.ai instances don't auto-stop on crash** — must set hard timeouts and actively monitor. Lost ~$15 to zombie instances.
- **Mac Mini fish shell corrupts binary streams** — scp/rsync fail with `[<u` escape sequences. Use R2 as intermediary.
- **Never run SSH commands as Claude Code background tasks** — orphaned connections cause double-launches.

### CodinGame Specifics

- **100K character limit** is the binding constraint, not timing. f16 + Unicode BMP encoding gives 1 char/weight (5.3x denser than base64).
- **GCC pragmas are mandatory** for C submissions: `#pragma GCC optimize("O3,unroll-loops,fast-math")` + `#pragma GCC target("avx2,fma")` give 25-30x speedup.
- **Variable board sizes** — height 10-24, width = round(h*1.8) even. Must zero-pad to (19,24,44).
- **Arena parallelism inflates timing** — always use --jobs 2 for authoritative evaluation, not all CPUs.
- **CG has numpy 1.23.2** with Python 3.11 — enables numpy-based NN inference.

## What I'd Do Differently

1. **Start with C/C++ from day one.** The Rust bot was well-engineered but the submission flattening and character counting overhead was unnecessary friction. C is simpler for competition bots.

2. **Validate ML ROI with the smallest possible experiment.** Train a tiny teacher on 500 seeds, see if it helps. If the improvement is marginal, don't scale 100x.

3. **Invest in deeper search, not better eval.** 2-ply maximin with a simple eval would likely outperform 1-ply maximin with a perfect eval. Search depth was the bottleneck, not eval quality.

4. **Set a hard dollar budget before starting.** "I'll spend up to $50 on GPU compute" prevents the scope creep that turned a learning exercise into a $300 bill.

5. **Use ChatGPT Pro / o1 earlier for debugging.** The PUCT diagnosis saved hours. Should have used it for the initial architecture decision too.

## Final Standings

- **Peak rank**: Silver 73rd / 1,099 in league (508th / 2,144 global)
- **Final rank**: Silver ~124th / 1,107 in league (578th / 2,161 global)
- **Bot type**: Pure C maximin heuristic, no neural network
- **Code size**: ~60K characters (well under 100K limit)
- **Total cost**: ~$310 (mostly ML pipeline that didn't ship)

The competition was a great learning experience. The ML pipeline, while not competitive, taught me a huge amount about distributed training, knowledge distillation, quantization, and cloud GPU economics. But if the goal was pure leaderboard rank, I should have spent those 2 weeks tuning heuristics and implementing deeper search instead.

---

*"The best eval function is the one you can iterate on in 2 minutes."*
