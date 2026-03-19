---
name: feedback_distill_logging
description: Always add logging and checkpointing before launching long GPU jobs
type: feedback
---

NEVER launch long-running GPU training jobs without per-epoch logging and checkpointing.

**Why:** Distill03 QAT jobs were launched on 36.8M rows (8 epochs × 287K batches) with zero mid-training output and no checkpointing. First run timed out at 1h, second run had 3h timeout but still no visibility into progress. If it times out again, all compute is wasted.

**How to apply:** Before launching ANY training job:
1. Verify the training function has per-epoch (or at minimum per-N-batches) logging
2. Verify checkpointing is enabled so partial progress survives timeouts
3. Set timeout to at least 2x the estimated wall time
4. Don't assume code is "ready to launch" just because it passes syntax checks — review the actual training loop for observability
