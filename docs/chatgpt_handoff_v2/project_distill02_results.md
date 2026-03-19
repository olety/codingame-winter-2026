---
name: project_distill02_results
description: Distill02 results from 128ch teacher (epoch 3), soft target pipeline, arena outcomes, quantization tests
type: project
---

## Distill02 Pipeline (2026-03-19)

**Teacher:** 128ch/8-block, epoch 3, val_corr=0.6728, pacc=90.32% (early stopped at epoch 6)
**Soft targets:** 2M rows from R2 bitpacked, stored as compact .npz on Modal Volume `/data/distill02/augmented/`
**Cost:** ~$3 total (A100 soft targets + 10× L40S distillation)

### Student Results

| Model | T | α | val_corr | pacc | Use |
|-------|---|---|----------|------|-----|
| 24ch/3L | 2.0 | 0.3 | 0.6245 | 77.85% | Rust backup |
| 32ch/3L | 2.0 | 0.3 | 0.6245 | 78.38% | Rust primary |
| 48ch/3L | 2.0 | 0.3 | 0.6280 | 79.32% | Python |
| 64ch/3L | 2.0 | 0.3 | 0.6385 | 80.08% | Python primary |

**Best hyperparams:** T=2.0, α=0.3 consistently wins across all sizes.

### Arena Results (32ch vs 12ch incumbent)

| Config | W/D/L | body_diff | Status |
|--------|-------|-----------|--------|
| 32ch prior=0.03 | 179/104/229 | -1.83 | REJECTED (prior hurts at 78% pacc) |
| 32ch prior=0.0 lm=0.08 | 169/203/140 | +0.78 | ACCEPTED |
| 32ch prior=0.0 lm=0.15 | 152/208/152 | +0.14 | ACCEPTED (barely) |

**Key finding:** prior_mix hurts at <85% pacc. leaf_mix 0.08-0.15 all within noise. The 32ch is roughly equal to 12ch incumbent — better NN offset by slightly slower inference.

### CG Timing (32ch on CG hardware)
- 50 inferences in 540.8ms = **10.82ms/call**
- Much faster than expected, plenty of headroom

### Python Bot (pure-NN, no search)
- 64ch beats Boss 5W/1D/2L
- Goes **0-16** against any Rust search bot
- Search is essential; 80% pacc insufficient without it

### Quantization Tests (post-training, no QAT)
- **int8:** 88-90% policy agreement with f16 baseline. Usable but ~10% accuracy loss.
- **int4:** 0-18% policy agreement. Completely destroyed without QAT.

### Character Budget Analysis

| Model | f16 chars | int4 chars | + Rust 65K | + C ~38K |
|-------|-----------|-----------|------------|----------|
| 48ch | 51K | 13K | 78K / — | 51K / — |
| 64ch | 86K | 22K | — / 87K | 60K / — |
| 80ch | 134K | 34K | — / 99K | 72K |
| 96ch | 192K | 49K | — / — | **87K** |

**Why:** f16=1 char/param, int4=0.25 char/param. C saves ~25-30K vs Rust.

### Next Steps
- 96ch int4 QAT + C rewrite is the high-value path
- Current teacher (90% pacc) is sufficient for 96ch student
- Python bot infrastructure ready for future pure-NN approaches
