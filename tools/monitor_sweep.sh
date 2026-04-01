#!/bin/bash
# Monitor vast.ai sweep instances — polls every 10 min, alerts when done or failed
SWEEP_ID="sweep_128ch_v2"
INSTANCES=(33000593 33000598 33000601 33000629 33000637)
NAMES=("lr0.005" "lr0.01" "lr0.02" "lr0.04" "wd1e-3")
MAX_HOURS=4  # kill instances after this many hours (safety net)
CHECK_INTERVAL=600  # 10 minutes

start_time=$(date +%s)

echo "=== Monitoring sweep $SWEEP_ID ==="
echo "Instances: ${INSTANCES[*]}"
echo "Max runtime: ${MAX_HOURS}h, checking every $((CHECK_INTERVAL/60))min"
echo ""

while true; do
    elapsed_hrs=$(echo "scale=1; ($(date +%s) - $start_time) / 3600" | bc)
    all_done=true
    echo "--- $(date '+%H:%M:%S') (${elapsed_hrs}h elapsed) ---"

    for i in "${!INSTANCES[@]}"; do
        id=${INSTANCES[$i]}
        name=${NAMES[$i]}

        status=$(vastai show instances --raw 2>/dev/null | python3 -c "
import sys,json
for inst in json.load(sys.stdin):
    if inst['id'] == $id:
        gpu = inst.get('gpu_util', 0) or 0
        status = inst.get('actual_status', 'unknown')
        print(f'{status}|{gpu:.0f}')
        break
else:
    print('destroyed|0')
" 2>/dev/null)

        inst_status=$(echo "$status" | cut -d'|' -f1)
        gpu_util=$(echo "$status" | cut -d'|' -f2)

        if [ "$inst_status" = "running" ]; then
            all_done=false
            last_log=$(vastai logs $id --tail 1 2>/dev/null | grep -v "waiting on logs" | tail -1)
            echo "  $name (#$id): RUNNING gpu=${gpu_util}% | ${last_log:0:120}"
        elif [ "$inst_status" = "destroyed" ]; then
            echo "  $name (#$id): DONE (destroyed)"
        else
            all_done=false
            echo "  $name (#$id): $inst_status gpu=${gpu_util}%"
        fi
    done

    # Safety: kill all instances if over max hours
    elapsed_secs=$(($(date +%s) - start_time))
    if [ $elapsed_secs -gt $((MAX_HOURS * 3600)) ]; then
        echo ""
        echo "MAX RUNTIME (${MAX_HOURS}h) EXCEEDED — destroying all instances!"
        for id in "${INSTANCES[@]}"; do
            vastai destroy instance $id 2>/dev/null
        done
        echo "Check results: python3 -m tools.sweep_vastai --results $SWEEP_ID"
        break
    fi

    if $all_done; then
        echo ""
        echo "ALL INSTANCES COMPLETE. Check results:"
        echo "  python3 -m tools.sweep_vastai --results $SWEEP_ID"
        break
    fi

    echo ""
    sleep $CHECK_INTERVAL
done

echo "DONE — monitor exiting."
