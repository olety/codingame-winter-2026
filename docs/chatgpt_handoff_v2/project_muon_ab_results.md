---
name: Muon vs AdamW A/B test results
description: Phase B A/B test on Modal A100 — Muon wins decisively over AdamW on 128ch/8-block teacher (103K samples)
type: project
---

## Muon vs AdamW A/B Test (2026-03-14)

Run ID: `muon_ab_1773463969`
Dataset: 103K samples from 2K merged seeds (single shape due to pre-padding loader)
Model: 128ch / 8 res blocks SE teacher
GPU: 2× A100 40GB (parallel), 20 epochs, batch 256

### Results

| Metric | AdamW | Muon | Delta |
|--------|-------|------|-------|
| Val MAE | 0.3288 | **0.2976** | -9.5% |
| Val Correlation | 0.4102 | **0.4551** | +10.9% |
| Val Policy Acc | 67.97% | **74.98%** | +10.3% |
| Train Value Loss | 0.00859 | 0.00183 | -78.7% |
| Train Policy Loss | 0.0593 | 0.00706 | -88.1% |
| Wallclock | 60.3min | 68.4min | +13.4% |

**Winner: Muon** — better on every metric. Slightly slower (~8min) but well worth it.

### Muon config used
- Optimizer: `SingleDeviceMuonWithAuxAdam` from `git+https://github.com/KellerJordan/Muon`
- Muon LR: 0.02 (2D+ weight matrices)
- AdamW LR: 3e-4 (1D biases/gains)
- Momentum: 0.95

### Key findings during test
1. Game has variable map sizes: height 10-24, width = round(h×1.8) even. Self-play produced two sizes: (19,22,40) 56% and (19,23,42) 44%.
2. Fixed by zero-padding all grids to max (19,24,44) in both Python dataset loader and Rust inference.
3. Dataset had 74 corrupt JSONL lines (mid-write during merge) — handled by try/except.

### Saved artifacts
- Full JSON: `data/ab_test_muon_ab_1773463969.json`
- Modal Volume checkpoints: `/data/muon_ab_1773463969_{adamw,muon}/teacher/`

**Why:** Muon is confirmed as the optimizer for Phase C (512ch teacher). The margin is large enough to be confident.
**How to apply:** Use Muon with LR=0.02 for all future teacher training. AdamW at 3e-4 for 1D params.
