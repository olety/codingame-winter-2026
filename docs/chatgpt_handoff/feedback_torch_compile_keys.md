---
name: feedback_torch_compile_keys
description: torch.compile checkpoints have _orig_mod. prefix on state dict keys — strip when loading into uncompiled model
type: feedback
---

Strip `_orig_mod.` prefix from state dict keys when loading torch.compile'd checkpoints into uncompiled models.

**Why:** The H100 teacher was saved from a torch.compile'd model. All keys had `_orig_mod.` prefix. Loading into an uncompiled TeacherHybridNet failed with missing keys.

**How to apply:**
```python
if any(k.startswith("_orig_mod.") for k in sd):
    sd = {k.removeprefix("_orig_mod."): v for k, v in sd.items()}
```
