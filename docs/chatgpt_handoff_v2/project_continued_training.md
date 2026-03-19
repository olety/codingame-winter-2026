---
name: continued pre-training support
description: Checkpoint format updated to save full state (model+optimizer+epoch), enabling warm-restart continued training with new data
type: project
---

## Continued Pre-Training Support (added 2026-03-17)

### What changed
- Epoch checkpoints (`teacher_epoch{N}.pt`) now save full state: `model_state_dict`, `optimizer_state_dict`, `epoch`
- Final model (`teacher_model.pt`) stays as bare `state_dict` (used for distillation/export)
- Backward compatible: loading old bare `state_dict` checkpoints still works (auto-detected)

### How to use
Add `continue_training: true` to the training spec (or `--continue-training` CLI flag):
```bash
--resume-checkpoint teacher_epoch20.pt --continue-training --epochs 10
```
This will:
1. Load model weights + optimizer state from the checkpoint
2. Reset the LR schedule (fresh cosine over the new epoch count — warm restart)
3. Continue epoch numbering from where it left off (e.g., epochs 21-30)

### Without `continue_training` (default)
Only model weights are loaded, optimizer + scheduler start fresh. This is the existing behavior for fine-tuning or transfer learning.

### Key files
- `python/train/outerloop/train_model.py` — `train_teacher_from_spec()`: resume logic, checkpoint saving, epoch loop

**Why:** Generating more selfplay data is ongoing. Rather than retraining from scratch each time, we can resume from the best checkpoint with a larger dataset.
**How to apply:** After generating more data, pack new shards with `--append`, then launch training with `--resume-checkpoint <best_ckpt> --continue-training --epochs N`.
