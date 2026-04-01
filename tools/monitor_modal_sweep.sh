#!/bin/bash
# Monitor Modal sweep — polls every 5 min, prints training progress
APP_IDS=(ap-jXfsWYakoGhgn78tzwlz71 ap-OsADJ09qVbM9idTcdaHBgU ap-XBdbDfWhNknsO6T4DiZiEj ap-OIf8QIKHTj89tUJQrHGrUX ap-ckpp2GHNKG09zAxN6LyB5m)
CHECK_INTERVAL=300  # 5 minutes
MAX_HOURS=4

start_time=$(date +%s)

echo "=== Modal Sweep Monitor ==="
echo "Apps: ${#APP_IDS[@]}"
echo "Checking every $((CHECK_INTERVAL/60))min, max ${MAX_HOURS}h"
echo ""

while true; do
    elapsed_hrs=$(echo "scale=1; ($(date +%s) - $start_time) / 3600" | bc)
    echo "--- $(date '+%H:%M:%S') (${elapsed_hrs}h elapsed) ---"

    all_done=true
    active_count=0

    for app_id in "${APP_IDS[@]}"; do
        state=$(modal app list --json 2>/dev/null | python3 -c "
import sys, json
for a in json.load(sys.stdin):
    if a['App ID'] == '$app_id':
        print(a['State'])
        break
else:
    print('unknown')
" 2>/dev/null)

        if [ "$state" = "ephemeral" ]; then
            all_done=false
            active_count=$((active_count + 1))
            # Get last training log line
            last_line=$(modal app logs "$app_id" 2>&1 &
            LOG_PID=$!
            sleep 8
            kill $LOG_PID 2>/dev/null
            wait $LOG_PID 2>/dev/null)

            # Extract last epoch/batch line
            progress=$(echo "$last_line" | grep -E "\[teacher\] Epoch" | tail -1)
            if [ -n "$progress" ]; then
                echo "  $app_id: $progress"
            else
                echo "  $app_id: RUNNING (no batch logs yet)"
            fi
        elif [ "$state" = "stopped" ]; then
            echo "  $app_id: COMPLETED"
        else
            echo "  $app_id: $state"
        fi
    done

    echo "  Active: $active_count / ${#APP_IDS[@]}"

    # Safety timeout
    elapsed_secs=$(($(date +%s) - start_time))
    if [ $elapsed_secs -gt $((MAX_HOURS * 3600)) ]; then
        echo ""
        echo "MAX RUNTIME (${MAX_HOURS}h) — stopping remaining apps"
        for app_id in "${APP_IDS[@]}"; do
            modal app stop "$app_id" 2>/dev/null
        done
        break
    fi

    if $all_done; then
        echo ""
        echo "ALL APPS COMPLETE!"
        echo "Check local results: ls /tmp/sweep_lr*.log /tmp/sweep_wd*.log"
        break
    fi

    echo ""
    sleep $CHECK_INTERVAL
done

echo "Monitor exiting at $(date '+%H:%M:%S')"
