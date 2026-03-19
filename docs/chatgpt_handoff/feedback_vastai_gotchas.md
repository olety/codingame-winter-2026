---
name: vast.ai operational gotchas
description: Hard-won lessons from vast.ai instance management — parsing, VRAM filtering, repo access, onstart limits, monitoring
type: feedback
---

## Vast.ai CLI Gotchas

1. **`vastai create instance` output is NOT JSON** — it's `Started. {'success': True, ...}` (Python dict with prefix). Must strip prefix and use `ast.literal_eval`, not `json.loads`.

2. **`success: False` still returns exit code 0** — always check the `success` field in the response dict, not just the return code. Failing to do this silently creates rogue instances.

3. **A100 SXM4 comes in 40GB and 80GB variants** — always filter `gpu_ram>=76` when the model needs >40GB VRAM. The 512ch/16-block teacher (78M params) at bs=512 needs ~45GB with bf16 autocast.

4. **`--onstart-cmd` has practical length limits** — long shell scripts fail silently. Upload the script to R2 and use a short bootstrap command that downloads and runs it.

5. **Repo must be public or use auth** — `git clone` in onstart fails silently with `|| true`. The repo is at `oneiron-dev/codingame-winter-2026` (not `olety/WinterChallenge2026-Exotec`). Code must be pushed before launching instances.

6. **`datacenter=True` filter is too restrictive** — most 80GB A100 SXM4 are community hosts. Removing this filter gives 3-10× more offers. Use `reliability>0.95` instead for quality.

7. **Offers get snatched fast** — search results may be stale by the time you create. Always try multiple offers per config with fallback logic.

8. **bs=1024 OOMs on 80GB A100** — 512ch/16-block teacher at bs=1024 needs >80GB VRAM. Max safe batch size on A100 80GB is 512. On H100 80GB probably same. Would need H200 (141GB) for bs=1024.

9. **Vast.ai log streaming lags badly** — logs may show stale output for 30+ min while GPU is at 99.9% utilization. Check `gpu_util` from `vastai show instances --raw` instead of relying on logs.

10. **`BitpackedDataset` has no `.rows` — `len(dataset.rows)` crashes** — use `len(dataset)` instead. This caused silent training failures where the crash happened after epoch completion during metrics serialization, wasting the entire run.

11. **MUST actively monitor instances and set hard timeouts** — don't rely on background monitors that check every 2 min. Set `vastai` instance `--max-duration` or implement a watchdog that destroys instances after expected completion time + buffer. Instances left running after crashes burn credits.

**Why:** Lost ~$15 and 4+ hours across multiple failed sweep launches. Instances ran for hours after crashing because monitoring was too passive.
**How to apply:** Always use `gpu_ram>=76`, `ast.literal_eval` for parsing, R2-hosted scripts, verify git clone succeeds, max bs=512 on 80GB, check gpu_util not logs, test training code locally before launching, set instance timeouts, destroy instances immediately when not needed.
