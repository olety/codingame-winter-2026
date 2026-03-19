---
name: remote selfplay management
description: Lessons on managing selfplay workers across multiple machines — SSH, process management, CC background jobs
type: feedback
---

## Remote Selfplay Management Rules

1. **NEVER run remote SSH commands as CC background tasks** — if CC restarts or the session compacts, orphaned SSH connections cause double-launches that waste CPU. Use `nohup` on the remote machine to detach, then immediately return from SSH.

2. **Kill order matters: parent → xargs → children** — `distributed_selfplay.sh` spawns `xargs -P N`, which spawns `selfplay_export` workers. If you kill children first, xargs respawns them. Kill the parent script, then xargs, then stragglers.

3. **Double-launch creates orphan parents** — if you launch the script twice, both parents scan seeds independently and one spawns workers. The scanning parent burns CPU doing nothing useful. Always verify only 1 parent exists per machine after launch.

4. **Verify process hierarchy after every launch:**
```bash
# Parents (should be exactly 1 per machine):
ps aux | grep distributed_selfplay | grep -v grep | wc -l
# Workers (should match --workers N):
ps aux | grep selfplay_export | grep -v grep | wc -l
```

5. **Fish shell (Arch, Mac Mini) gotchas:**
   - SSH exit code 255 doesn't always mean failure — `pkill` returns non-zero when it kills the shell's own parent
   - Use newlines not semicolons for multi-command SSH
   - `set -x PATH $HOME/.cargo/bin $PATH` instead of `export PATH=...`
   - For complex logic, wrap in `bash -c "..."` since the selfplay script is bash anyway

6. **Mac Mini SSH shows `[<u` escape sequences** — harmless terminal escape codes from zsh/fish, ignore them

**Why:** Lost hours to double-launched workers (28 instead of 14 on Contabo, 16 instead of 8 on Mac Mini) burning CPU and potentially corrupting data. CC background SSH tasks caused repeated double-launches when killed and restarted.
**How to apply:** Always `nohup` + detach on remote, never CC background for SSH. Verify 1 parent + correct worker count after every launch.
