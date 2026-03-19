---
name: scaling plan v2
description: Plan for scaled teacher-student pipeline — 20K seeds, 512ch teacher with Muon, dual Rust/Python students
type: project
---

## Plan v2: Scaled Teacher-Student Pipeline (2026-03-13)

### Phase A — Self-play data generation (local, $0)
- 20K seeds on Contabo VPS (16 cores) + Mac Mini M4 (10 cores) overnight
- ~4.6M training positions, ~7 hours
- Self-play bot: current hybrid (value-mid-12ch-distill01)
- Cross-compile Rust binary for x86_64 (Contabo) and aarch64 (Mac Mini)
- Checkpointable: one JSONL per seed, resume = skip existing files
- Data flow: Mac Mini rsyncs → Contabo (hub, 190GB free) → R2 bucket → Modal pulls for training
- Contabo has Cloudflare CLI (wrangler) for R2 uploads

### Phase B — Muon vs AdamW A/B test (Modal, ~$2) ✅ COMPLETE
- Trained 2× 128ch/8-block SE teachers on 103K samples (2K seeds)
- **Muon wins decisively**: val_corr 0.4551 vs 0.4102 (+10.9%), policy_acc 74.98% vs 67.97% (+10.3%)
- Both on A100 40GB, 20 epochs, ~60-69 min each
- Results: `data/ab_test_muon_ab_1773463969.json`

### Phase C — Teacher training (Modal H100, ~$28) ✅ COMPLETE
- 128ch/8-block SE teacher on 36.8M positions (164K seeds, bitpacked on R2)
- **Muon optimizer**, bs=4096, lr=0.005, wd=1e-3, cosine LR
- Early stopped at epoch 6 (patience=3), best at epoch 3
- **Best**: val_corr=0.6728, val_pacc=90.32% (vs 0.52/61% from distill01 teacher)
- Checkpoint: `h100_full_run_v2/teacher/checkpoints/teacher_epoch3.pt` on Modal Volume
- 512ch teacher deferred — 128ch saturated, try distillation first

### Phase D — Distillation + arena screening (~$3) ⬅ NEXT
- Soft targets from 128ch epoch 3 teacher
- Distill into:
  - 24ch/3L + 32ch/3L students → Rust bot (search + NN, f16, ~81-91K chars)
  - 48ch/3L + 64ch/3L students → Python bot (numpy NN, f16, ~57-92K chars)
- Arena screen all vs current incumbent
- Submit winner

### Phase E — Python bot (parallel with C/D)
- Minimal Python NN bot: I/O + feature encoding + numpy CNN + int8 decode
- ~5K chars code, ~95K chars for weights
- Pure policy (no search) or simple 1-ply if feasible

### Encoding pipeline
- f16 Unicode: 1 char/weight (current, for Rust ≤32ch)
- int8 Unicode: 0.5 char/weight (for larger models, enables 48ch Rust / 96ch Python)
- Conv optimization: 3×3 unroll + interior/edge split (already done)

### Key insight
- Teacher quality > student size > encoding tricks
- Data (20K seeds) > model size (512ch) > optimizer (Muon)
- CG timing is NOT a constraint (4ms for 32ch on CG hardware)
- 100K char limit IS the constraint — f16/int8 + Python unlocks bigger models

**Why:** The first hybrid (+0.94 body diff) was limited by teacher quality (128ch on 114K samples, value corr 0.52). Scaling data 40× and teacher 4× should dramatically improve distillation quality.
**How to apply:** Execute phases A→B→C→D in order. Phase E can be built in parallel.
