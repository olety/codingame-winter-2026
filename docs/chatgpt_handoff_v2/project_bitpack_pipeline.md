---
name: bitpacked pipeline status
description: Sharded bitpacked binary dataset format (v2), R2 pipeline, vast.ai training setup — current status and costs
type: project
---

## Sharded Bitpacked Dataset Pipeline (updated 2026-03-17 afternoon)

### Format v2 — sharded (2,528 bytes/row)
Each row is 2,528 bytes across `shard_NNN_data.bin` files (400K rows/shard ≈ 1GB each):
| Offset | Bytes | Field | Type |
|--------|-------|-------|------|
| 0 | 2,508 | grid | packbits(grid[19,24,44].flatten()) |
| 2,508 | 12 | scalars | float16[6] |
| 2,520 | 2 | value | float16 |
| 2,522 | 4 | policy | int8[4] |
| 2,526 | 2 | weight | float16 |

Meta (16 bytes/row per `shard_NNN_meta.bin`): uint64 hash + uint32 game_id_hash + int32 seed.

### Current data (2026-03-17)
- **36.77M unique positions** across **93 shards** on R2 (93 GB)
- Sources: local (100K-197K), Mac Mini (200K-216K), Contabo (300K-313K), Arch (400K-420K), plus old 1-100K
- Params/sample ratio: **2.1:1** (78M params / 36.8M positions)
- `header.json` now tracks `processed_files` for fast future appends

### Incremental workflow
```bash
# Append new data (skips already-processed files by name, dedupes by hash)
python3 -m python.train.outerloop.pack_dataset \
  --input data/new_seeds/ --output-dir data/bitpacked --append --workers 12

# Sync to R2 (only uploads new/changed shard files)
aws s3 sync data/bitpacked/ s3://snakebot-data/bitpacked/ \
  --endpoint-url https://70ae12731f3e503777d9f59a6c2c18da.r2.cloudflarestorage.com
```

### R2 bucket layout (`s3://snakebot-data/`)
- `raw/local/` — M4 Max selfplay seeds (97K)
- `raw/contabo/` — Contabo VPS seeds (50K old + 13K new)
- `raw/macmini/` — Mac Mini seeds (50K old)
- `raw/macmini_new/` — Mac Mini seeds (16.7K, 200K range)
- `bitpacked/` — 93 shards, 36.77M rows, 93GB total (**uploaded**)

### Sweep status
- Sweep `sweep_1773721231` launching on vast.ai (5 configs: lr=0.005/0.01/0.02/0.04, wd=1e-3)
- All configs use: 512ch/16-block, Muon optimizer, bs=512, 2 epochs
- bs=1024 removed (OOMs on 80GB A100)

### Code
- `python/train/outerloop/pack_dataset.py` — sharded packer with `processed_files` tracking
- `BitpackedDataset` in `dataset.py` — v1/v2, memory-mapped, ~600MB RAM for 36M rows
- `train_teacher_from_spec()` — auto-detects bitpacked, supports `continue_training` warm restart
- `tools/sweep_vastai.py` — parallel hyperparam sweep across vast.ai instances

### Cost comparison for 512ch teacher training (20 epochs)
| Platform | GPU | $/hr | Est. total |
|----------|-----|------|-----------|
| Vast.ai | A100 SXM4 80GB | $0.62-0.91 | **~$8-12** |
| Modal | A100 40GB | $2.10 | ~$25 |

**Why:** Sharding enables incremental updates — new data only uploads new ~1GB shard files.
**How to apply:** Use `--append` for new data, `aws s3 sync` for upload. Train on vast.ai A100 SXM4 80GB (MUST be 80GB). Always test locally first.
