---
name: modal gpu pricing
description: Current Modal GPU pricing (March 2026) for training cost estimation
type: reference
---

## Modal GPU Pricing (March 2026)

| GPU | VRAM | $/hr |
|-----|------|------|
| T4 | 16 GB | $0.59 |
| L4 | 24 GB | $0.80 |
| A10 | 24 GB | $1.10 |
| L40S | 48 GB | $1.95 |
| A100 40GB | 40 GB | $2.10 |
| A100 80GB | 80 GB | $2.50 |
| H100 | 80 GB | $3.95 |
| H200 | 141 GB | $4.54 |
| B200 | 192 GB | $6.25 |

CPU: $0.047/core/hr. Memory: $0.008/GiB/hr. Per-second billing.

Price drops since Jan 2025: H100 -13%, A100 80GB -26%, A100 40GB -24%.

**How to apply:** A100 40GB ($2.10) is the sweet spot — only $0.15 more than L40S but much better tensor cores for training. Use for teacher training and Muon A/B test. H100 overkill for 78M param teacher.
