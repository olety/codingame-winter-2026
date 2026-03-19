---
name: 128ch teacher training — full results
description: 128ch/8-block teacher on 36.8M positions — sweeps, BS test, full run with early stopping
type: project
---

## 128ch/8-block Teacher Sweep (2026-03-17)

### LR/WD Sweep (bs=1024, 2 epochs, H100)
| Rank | Config | ploss | vloss |
|---|---|---|---|
| **1** | lr=0.02, wd=1e-3 | **0.2285** | **0.0503** |
| 2 | lr=0.005, wd=1e-4 | 0.2409 | 0.0505 |
| 3 | lr=0.01, wd=1e-4 | 0.2466 | 0.0507 |

### BS Test (1 epoch, H100)
| BS | LR | vloss | ploss | Speed | Result |
|---|---|---|---|---|---|
| 8192 | 0.005 | — | — | — | OOM |
| **4096** | **0.005** | **0.0526** | **0.3024** | 0.36s/batch | **Best throughput** |
| 4096 | 0.02 | 0.0531 | 0.3168 | 0.46s/batch | Worse convergence |
| 2048 | 0.005 | 0.0526 | 0.2974 | 0.26s/batch | Best loss, slower throughput |

### Full Run — COMPLETE (2026-03-18)
- **Config**: bs=4096, lr=0.005, wd=1e-3, Muon, 19 epochs, H100
- **Early stopping**: patience=3 on val_corr, triggered at epoch 6
- **Total time**: 362.8 min (~6 hrs), cost ~$28
- **Checkpoints**: epochs 1-6 on Modal Volume `snakebot-datasets` at `h100_full_run_v2/teacher/checkpoints/`

| Epoch | val_corr | val_pacc | vloss | ploss | Early Stop |
|-------|----------|----------|-------|-------|------------|
| 1 | 0.6634 | 89.47% | — | — | — |
| 2 | 0.6718 | 90.09% | 0.0506 | 0.2375 | — |
| **3** | **0.6728** | **90.32%** | **0.0500** | **0.2223** | **Best** |
| 4 | 0.6725 | 90.32% | 0.0497 | 0.2132 | 1/3 |
| 5 | 0.6720 | 90.43% | 0.0493 | 0.2065 | 2/3 |
| 6 | ~0.666 | 90.49% | — | — | 3/3 stopped |

**Best checkpoint**: `teacher_epoch3.pt` (val_corr=0.6728, val_pacc=90.32%)
**Key finding**: 128ch architecture saturates at epoch 3 with 36.8M positions. Training loss keeps dropping but val_corr declines — overfitting. The 128ch model has extracted all it can from this data volume.

**Why:** These results guide distillation (use epoch 3 checkpoint) and inform whether a 512ch teacher would benefit from more capacity.
**How to apply:** Use epoch 3 checkpoint for distillation. Consider 512ch teacher only if 128ch distillation results are insufficient.
