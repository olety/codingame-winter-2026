---
name: self-play data generation status
description: Distributed self-play generation across 4 machines — seed ranges, throughput, and collection pipeline
type: project
---

## Distributed Self-Play Generation (updated 2026-03-17 afternoon)

### Status: paused on Contabo + Arch, local stopped (197K/200K done), Mac Mini still running

| Machine | SSH | Workers | Seed range | Seeds done | Status |
|---|---|---|---|---|---|
| M4 Max (local) | local | 12 | 100,001-200,000 | ~97K | **stopped** (nearly done) |
| Mac Mini M4 | oletymac | 8 | 200,001-300,000 | ~16.7K | **running** |
| Contabo VPS | vps | 14 | 300,001-400,000 | ~13K | **stopped** (restart evening) |
| Arch (Ryzen 7 8745HS) | lexi@192.168.11.16 | 14 | 400,001-500,000 | ~21K | **stopped** (restart evening) |

Previously completed: 1-50K (Contabo), 50K-100K (Mac Mini) — all packed into bitpacked dataset.

### Seed range allocation — NEVER OVERLAP
- 1-50K: Contabo (done, packed)
- 50K-100K: Mac Mini (done, packed)
- 100K-200K: Local (done, packed)
- 200K-300K: Mac Mini (running, 16.7K packed)
- 300K-400K: Contabo (stopped, 13K packed)
- 400K-500K: Arch (stopped, 21K packed)

### Machine notes
- **Mac Mini**: zsh escape sequences (`[<u`) corrupt binary streams (tar, scp). Use R2 as intermediary: tar locally → push to R2 → download from R2. Or create tar on remote, scp the file.
- **Arch**: fish shell, no rsync installed. Use `tar cf - | tar xf -` over SSH. `bash -c` for complex commands.
- **Contabo**: fish shell, slowest (shared CPU VPS). Kill workers via parent first.
- **All machines**: NEVER launch from CC background jobs. Always `nohup` + detach on remote.

### Collection pipeline
1. Each machine has AWS CLI + R2 creds configured
2. `aws s3 sync data/selfplay/ s3://snakebot-data/raw/<machine>/ --exclude "*.tmp"`
3. Download new seeds locally (rsync for LAN, R2 for Mac Mini due to escape sequences)
4. `pack_dataset.py --append` (skips already-processed files)
5. `aws s3 sync data/bitpacked/ s3://snakebot-data/bitpacked/`

**Why:** More data = better teacher. 36.8M positions gives 2.1:1 ratio for 78M param model.
**How to apply:** Restart Contabo + Arch in evening. Sync → pack → upload when ready. Never overlap seed ranges.
