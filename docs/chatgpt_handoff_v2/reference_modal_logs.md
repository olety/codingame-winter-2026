---
name: Modal and vast.ai log access
description: How to get training logs from Modal and vast.ai — CLI tricks for macOS
type: reference
---

## Modal Logs

**App logs** (replays full history then streams):
```bash
modal app logs <app_id>
```

**Container logs** (same behavior):
```bash
modal container list    # get container ID
modal container logs <container_id>
```

**Filtering trick** (macOS has no `timeout`):
```bash
perl -e 'alarm 120; exec @ARGV' -- modal app logs <app_id> 2>&1 | grep '\[teacher\]' | tail -10
```

**Key gotcha**: `tail -N` on modal logs only captures end of historical buffer, NOT live output. Always use `grep` to filter through the full stream.

**Web dashboard**: Most reliable for live viewing — `https://modal.com/apps/<workspace>/main/<app_id>`

**Other commands:**
```bash
modal app list           # list apps + state
modal app stop <app_id>  # stop running app
modal volume ls <name>   # inspect volume contents
```

## Vast.ai Logs

**SSH** (most reliable):
```bash
ssh -p <port> root@<ip> 'tail -f /var/log/onstart.log'
```

**GPU utilization** (reliable indicator — 99% = training, 0% = crashed):
```bash
vastai show instances --raw | python3 -c "import sys,json; [print(f'#{i[\"id\"]} gpu={i.get(\"gpu_util\",0):.0f}%') for i in json.load(sys.stdin)]"
```
