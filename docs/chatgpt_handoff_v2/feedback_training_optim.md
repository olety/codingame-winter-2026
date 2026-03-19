---
name: training optimization lessons
description: PyTorch CNN training optimizations — what works, what conflicts, compile mode choice
type: feedback
---

## Training Optimization Lessons (2026-03-18)

### What works
- **torch.compile(mode="reduce-overhead", dynamic=False)** — same training speed as max-autotune (0.36s/batch on H100) but compiles instantly vs 80+ min
- **cudnn.benchmark=True** + **torch.set_float32_matmul_precision("high")** — free speedup
- **non_blocking transfers** + **GPU-side loss accumulation** (.detach() not .item())
- **persistent_workers=True, prefetch_factor=3, drop_last=True** for DataLoader
- **bf16 autocast** with Muon optimizer
- **bisect for shard lookup** — O(log n) vs O(n) over 93 shards

### What conflicts
- **channels_last + Muon**: Muon's `.view()` fails on non-contiguous. Fix: fork to use `.reshape()`. ~15-20% speedup left on table.
- **Native torch.optim.Muon (2.9+)**: Only supports 2D params, crashes on 4D conv weights.
- **max-autotune**: 80+ min on fresh container, sometimes hangs. **Never use — reduce-overhead gives identical speed.**
- **A100 40GB + bs=4096**: OOM during torch.compile. Need A100 80GB or reduce BS.

### Compile mode decision
- 1-2 epochs: `use_compile: false`
- 3+ epochs: `compile_mode: "reduce-overhead"` (always)
- Never use max-autotune

### Memory requirements (Modal)
- H100/A100: 49152 MB minimum (48GB) — workers OOM at 32GB with 8 workers + mmap
- 192GB was wasting $1.15/hr in memory charges

**Why:** max-autotune wasted $6+ on stuck compile, 192GB memory wasted ~$83 on old workspace.
**How to apply:** Always use reduce-overhead. Always check memory= in modal_job.py before launching.
