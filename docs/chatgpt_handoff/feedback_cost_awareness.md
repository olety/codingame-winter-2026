---
name: cost awareness for cloud GPU
description: CRITICAL — always estimate and confirm costs before launching GPU jobs, never run parallel expensive jobs without approval
type: feedback
---

## Be extremely mindful of cloud GPU costs

**Why:** User got a $293 Modal bill ($119 out of pocket) from H100 runs + hidden memory charges. User is not funded — every dollar matters.

**How to apply:**
- **Always estimate total cost BEFORE launching** — include GPU + memory + CPU, not just GPU
- **Memory is a hidden cost**: Modal charges $0.008/GiB/hr. Memory-mapping 93GB dataset = ~$0.74/hr per container ON TOP of GPU cost
- **Never run 4 parallel H100 jobs** without explicitly telling user the per-hour burn rate
- **H100 effective cost with memory**: ~$4.70/hr not $3.95/hr. A100 40GB effective: ~$2.85/hr
- **Prefer A100 40GB over H100** for small models (128ch = 2.5M params) — throughput difference is minimal, cost is 40% less
- **Always confirm budget** before launching multi-hour runs
- **Add early stopping** to training code — don't burn GPU hours on diminishing returns
- **Download checkpoints and delete volumes** after runs complete to avoid storage charges
- **Set workspace budget limits** on Modal to prevent overruns
