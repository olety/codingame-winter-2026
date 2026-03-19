---
name: feedback_r2_mmap_perf
description: R2 CloudBucketMount memmap is extremely slow for random access — must preload data into RAM
type: feedback
---

Never use numpy memmap with random access on R2 CloudBucketMount. Each page fault is a network round-trip.

**Why:** BitpackedDataset uses np.memmap. Random access (shuffled training, random subsampling) causes millions of small network reads. 2M rows took forever vs ~12 min sequential.

**How to apply:**
- For soft target generation: preload sampled rows into tensors before inference
- For distillation training: preload raw rows into RAM during dataset init (~4.8GB for 2M rows)
- Sort random indices before sequential access to improve mmap read-ahead
- Use `num_workers=0` with R2 mmaps (multiprocessing + mmap doesn't work well)
