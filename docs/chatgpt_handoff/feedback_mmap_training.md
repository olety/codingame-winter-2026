---
name: feedback_mmap_training
description: Mmap works for training even when dataset >> RAM. Don't default to subsetting — use the full dataset.
type: feedback
---

Mmap works for training datasets larger than RAM. Don't reflexively subset or preload.

**Why:** 93GB bitpacked dataset on 64GB RAM M4 Max — mmap with random DataLoader access works at 75ms/step. On 16GB Mac Mini, mmap still works for the full 93GB dataset, just slower per step due to more page faults. The OS page cache handles it. Only preload when dataset fits comfortably in RAM AND you need the speed.

**How to apply:**
- Default to mmap (preload=False) for any dataset. It always works.
- Only use preload=True when dataset fits in ~50% of RAM and you want speed (e.g., 10M rows = 25GB on 64GB machine)
- NEVER assume a machine "can't handle" a dataset because of RAM. Mmap exists.
- DataLoader num_workers=0 required on macOS with mmap (spawn can't pickle mmap handles)
- `grouped_split_indices_bitpacked` must be vectorized (numpy hash-based), NOT a Python loop — 36.8M rows takes minutes in pure Python, <0.5s with numpy
