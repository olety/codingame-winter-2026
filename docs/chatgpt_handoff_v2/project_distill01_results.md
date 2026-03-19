---
name: distill01 run results
description: First distillation pipeline run (20260313-distill01) — C8 promoted as first hybrid to beat search-only incumbent
type: project
---

## Run 20260313-distill01 Results (2026-03-13)

**Outcome**: C8 (value-mid-12ch) promoted — first hybrid candidate to beat the search-only incumbent.

### Key findings

1. **Policy priors are harmful at current model quality.** Even at pm=0.03, the 61% accuracy policy prior hurts search. All prior-based candidates lost.
2. **Value-only integration works.** leaf_mix=0.08, prior_mix=0.0 achieves +0.94 heldout body diff over 512 authoritative matches vs the incumbent.
3. **The value head is the viable integration point.** Value correlation (0.52) is much more useful than policy accuracy (61%).
4. **Mix levels must be very low.** Gen 1 used pm=0.08-0.12 and lost by -3.75 to -5.19. Gen 2 at pm=0.03 or lm=0.08 approached parity.
5. **CPU contention distorts arena timing.** 14 parallel jobs inflated p99 from 40ms to 75ms. Running with 2 jobs gives accurate results.
6. **Distilled 12ch/3L student from 128ch/8-block SE teacher** — teacher trained 20 epochs on 114K self-play samples from 500 seeds.

### Promoted config
- Name: hybrid-value-mid-12ch-distill01
- Search: 6/8/3/3 (unchanged from incumbent)
- Hybrid: prior_mix=0.0, leaf_mix=0.08, value_scale=48.0
- Weights: rust/bot/weights/distill_c2_12ch_3L.json (12ch, 3 conv layers)

### Flattened submission
- `generate_flattened_submission.py` extended to support hybrid configs by embedding weights
- Compacted from ~236K -> 89K chars (comment strip + indent 4->1 space)
- Unicode encoding further reduces to ~72K chars for 12ch (2.67x denser than base64)
- Unlocks 16ch (78K) and 24ch (92K) models within 100K limit

**Why:** Understanding what worked/failed in distill01 guides future runs. Value-only integration + very low mixing is the path forward.
**How to apply:** Future distillation runs should explore leaf_mix 0.05-0.15 with prior_mix=0.0. Do NOT increase prior_mix above 0.03.
