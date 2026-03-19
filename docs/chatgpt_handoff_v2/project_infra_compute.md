---
name: compute infrastructure
description: Available machines for self-play and training — full specs, SSH, shells, storage, PyTorch status
type: reference
---

## Local Compute Fleet

| Machine | CPU | RAM | GPU | Disk Free | SSH | Shell | PyTorch |
|---------|-----|-----|-----|-----------|-----|-------|---------|
| M4 Max (main) | M4 Max 14-core | 64 GB | Metal/MPS | 59 GB | local | zsh | Yes (3.14) |
| Mac Mini M4 | M4 10-core | 16 GB | Metal/MPS | 54 GB internal, 1.1TB Oletydrive, 4.8TB Cinema | ssh oletymac | fish | Yes (2.8.0) |
| Contabo VPS | AMD EPYC 16-core (no SMT) | 62 GB | **None** | 158 GB | ssh vps | fish | Yes (2.10+cu128, CPU only) |
| Arch | Ryzen 7 8745HS | 30 GB | Radeon 780M iGPU (no CUDA) | 855 GB NVMe | ssh lexi@192.168.11.16 | fish | No (Python 3.14) |

## SSH / Shell Notes

- **Mac Mini fish shell corrupts binary streams** (scp, rsync fail with `[<u` escape sequences). Use R2 as intermediary or `cat file | ssh oletymac "bash -c 'cat > dest'"` for small files.
- **Contabo + Arch use fish shell** — no `&&` chaining, use `; ` or wrap in `bash -c '...'`. Use `status is-interactive` for interactive-only config.
- **VPS SSH host is `vps`** (not `contabo`)

## Training Capabilities

- **M4 Max + Mac Mini**: Both have MPS. Can train PyTorch models. M4 Max is fastest (40-core GPU vs 10-core).
- **Mmap works on local SSD for training** — no need to preload into RAM if dataset > RAM. Page cache handles it. Slower than preloaded (75ms vs 35ms/step on M4 Max) but works for any dataset size.
- **For datasets that fit in RAM**: preload is faster. 10M rows = 25GB preloaded, fits in M4 Max 64GB.
- **For datasets that DON'T fit in RAM**: mmap with no preload. 36.8M rows = 93GB — won't fit even M4 Max, but mmap works fine. On Mac Mini (16GB), mmap still works for full dataset, just slower per step.
- **DataLoader num_workers must be 0 with mmap** — macOS multiprocessing "spawn" can't pickle mmap handles. Single-process loading is fine.
- **VPS/Arch**: CPU-only training. 20-50x slower than MPS. Only useful for multi-day background jobs.

## Parallel Training Strategy

Train different model sizes on different machines simultaneously:
- M4 Max: largest model (96ch), preload subset or mmap full dataset
- Mac Mini: smaller model (48ch/64ch), mmap full dataset from Oletydrive
- Both train at $0

## Storage / Data Pipeline

- **87GB bitpacked dataset** on M4 Max local disk AND on R2
- **Mac Mini external SSDs**: Oletydrive (1.1TB free), Cinema (4.8TB free) — use for training data
- **R2**: all machines have AWS CLI + R2 creds. Mac Mini syncs from R2 (not rsync from M4 Max due to fish shell binary corruption)
- **Modal Volume**: teacher predictions, checkpoints. Download via `modal volume get`

**Why:** Local machines are free. Modal only needed for GPU work that requires CUDA (teacher inference at scale). MPS handles student distillation training.
**How to apply:** Train locally on Macs (free, MPS). Use R2 to move data between machines. Modal only for soft target generation or if CUDA-specific ops needed.
