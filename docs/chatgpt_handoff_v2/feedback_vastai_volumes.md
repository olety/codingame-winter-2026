---
name: use vast.ai volumes for dataset
description: Create persistent volume on vast.ai to avoid re-downloading 93GB dataset for every instance
type: feedback
---

Before launching the full training run on vast.ai, create a persistent volume with the bitpacked dataset pre-loaded. Each sweep instance currently downloads 93GB from R2 — wasteful and slow.

**Setup steps:**
1. `vastai search volumes` to find volume offer on an A100 SXM4 80GB host
2. `vastai create volume <offer_id> --size 150 --name snakebot-data`
3. Launch one instance with the volume, download data from R2 once
4. All future instances on that host mount the volume — zero download time

**Why:** Each instance wastes 10-20 min downloading 93GB. For a 20-epoch run that's fine (amortized), but for sweeps with multiple short instances it's a significant fraction of total runtime.
**How to apply:** Set up volume BEFORE launching full training run. Use for any future sweeps too.
