#!/usr/bin/env bash
# Deploy self-play to remote machines and kick off generation.
#
# Prerequisites:
#   - SSH access to machines (ssh contabo, ssh oletymac)
#   - Rust toolchain on each machine (or this script installs it)
#
# Usage:
#   ./tools/deploy_selfplay.sh setup     # Install Rust + clone repo on remotes
#   ./tools/deploy_selfplay.sh start     # Start self-play on both machines
#   ./tools/deploy_selfplay.sh status    # Check progress on both machines
#   ./tools/deploy_selfplay.sh collect   # Rsync Mac Mini → Contabo, merge all
#   ./tools/deploy_selfplay.sh upload    # Upload merged data to R2

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Machine config
CONTABO_HOST="contabo"
MACMINI_HOST="oletymac"
REMOTE_DIR="~/WinterChallenge2026-Exotec"
REMOTE_DATA_DIR="$REMOTE_DIR/data/selfplay"

# Seed distribution: 20K total
# Contabo: seeds 1-10000 (16 cores → 14 workers)
# Mac Mini: seeds 10001-20000 (10 cores → 8 workers)
CONTABO_SEED_START=1
CONTABO_SEED_END=10000
CONTABO_WORKERS=14

MACMINI_SEED_START=10001
MACMINI_SEED_END=20000
MACMINI_WORKERS=8

R2_BUCKET="snakebot-data"

cmd_setup() {
    echo "=== Setting up remote machines ==="

    for host in "$CONTABO_HOST" "$MACMINI_HOST"; do
        echo ""
        echo "--- $host ---"

        # Check if Rust is installed
        if ssh "$host" "command -v cargo" &>/dev/null; then
            echo "  Rust already installed"
        else
            echo "  Installing Rust..."
            ssh "$host" "curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y"
        fi

        # Clone or update repo
        if ssh "$host" "test -d $REMOTE_DIR/.git"; then
            echo "  Updating repo..."
            ssh "$host" "cd $REMOTE_DIR && git pull --ff-only"
        else
            echo "  Cloning repo..."
            REPO_URL=$(cd "$REPO_ROOT" && git remote get-url origin 2>/dev/null || echo "")
            if [[ -z "$REPO_URL" ]]; then
                echo "  ERROR: No git remote found. Push repo first or rsync manually."
                echo "  Manual rsync: rsync -avz --exclude target --exclude data $REPO_ROOT/ $host:$REMOTE_DIR/"
                continue
            fi
            ssh "$host" "git clone $REPO_URL $REMOTE_DIR"
        fi

        # Build
        echo "  Building selfplay_export (release)..."
        ssh "$host" "cd $REMOTE_DIR && source ~/.cargo/env 2>/dev/null; cargo build --release -p snakebot-bot --bin selfplay_export" 2>&1 | tail -3

        # Create data dir
        ssh "$host" "mkdir -p $REMOTE_DATA_DIR"

        echo "  $host ready!"
    done

    echo ""
    echo "=== Setup complete ==="
}

cmd_start() {
    echo "=== Starting self-play generation ==="

    echo ""
    echo "--- Contabo ($CONTABO_HOST): seeds $CONTABO_SEED_START-$CONTABO_SEED_END, $CONTABO_WORKERS workers ---"
    ssh "$CONTABO_HOST" "cd $REMOTE_DIR && source ~/.cargo/env 2>/dev/null; nohup bash tools/distributed_selfplay.sh \
        --seed-start $CONTABO_SEED_START --seed-end $CONTABO_SEED_END \
        --workers $CONTABO_WORKERS --out-dir $REMOTE_DATA_DIR \
        --no-build \
        > data/selfplay_contabo.log 2>&1 &
        echo \$!"
    echo "  Started on Contabo (check: ssh $CONTABO_HOST tail -f $REMOTE_DIR/data/selfplay_contabo.log)"

    echo ""
    echo "--- Mac Mini ($MACMINI_HOST): seeds $MACMINI_SEED_START-$MACMINI_SEED_END, $MACMINI_WORKERS workers ---"
    ssh "$MACMINI_HOST" "cd $REMOTE_DIR && source ~/.cargo/env 2>/dev/null; nohup bash tools/distributed_selfplay.sh \
        --seed-start $MACMINI_SEED_START --seed-end $MACMINI_SEED_END \
        --workers $MACMINI_WORKERS --out-dir $REMOTE_DATA_DIR \
        --no-build \
        > data/selfplay_macmini.log 2>&1 &
        echo \$!"
    echo "  Started on Mac Mini (check: ssh $MACMINI_HOST tail -f $REMOTE_DIR/data/selfplay_macmini.log)"

    echo ""
    echo "=== Both machines running. Monitor with: $0 status ==="
}

cmd_status() {
    echo "=== Self-play progress ==="

    for host in "$CONTABO_HOST" "$MACMINI_HOST"; do
        echo ""
        echo "--- $host ---"
        # Count completed seed files (gzipped)
        COUNT=$(ssh "$host" "ls -1 $REMOTE_DATA_DIR/seed_*.jsonl.gz 2>/dev/null | wc -l" 2>/dev/null || echo "0")
        SIZE=$(ssh "$host" "du -sh $REMOTE_DATA_DIR 2>/dev/null | awk '{print \$1}'" 2>/dev/null || echo "?")
        RUNNING=$(ssh "$host" "pgrep -f selfplay_export | wc -l" 2>/dev/null || echo "0")
        echo "  Completed seeds: $COUNT"
        echo "  Disk usage: $SIZE"
        echo "  Running workers: $RUNNING"
        # Show last log line
        if [[ "$host" == "$CONTABO_HOST" ]]; then
            LOG="data/selfplay_contabo.log"
        else
            LOG="data/selfplay_macmini.log"
        fi
        LAST=$(ssh "$host" "tail -1 $REMOTE_DIR/$LOG 2>/dev/null" || echo "(no log)")
        echo "  Last log: $LAST"
    done
}

cmd_collect() {
    echo "=== Collecting data to Contabo hub ==="

    echo "Rsyncing Mac Mini → Contabo..."
    ssh "$MACMINI_HOST" "rsync -avz --progress $REMOTE_DATA_DIR/ $CONTABO_HOST:$REMOTE_DATA_DIR/" 2>&1 | tail -5

    echo ""
    echo "Merging on Contabo..."
    ssh "$CONTABO_HOST" "cd $REMOTE_DIR && zcat $REMOTE_DATA_DIR/seed_*.jsonl.gz | gzip > data/selfplay_merged.jsonl.gz && echo 'Merged:' && ls -lh data/selfplay_merged.jsonl.gz"

    echo ""
    echo "=== Collection complete ==="
}

cmd_upload() {
    echo "=== Uploading to R2 ==="

    SIZE=$(ssh "$CONTABO_HOST" "ls -lh $REMOTE_DIR/data/selfplay_merged.jsonl.gz | awk '{print \$5}'" 2>/dev/null)
    echo "Merged file size: $SIZE"

    echo "Uploading to R2 bucket $R2_BUCKET..."
    ssh "$CONTABO_HOST" "cd $REMOTE_DIR && wrangler r2 object put $R2_BUCKET/selfplay_20k_merged.jsonl.gz --file data/selfplay_merged.jsonl.gz"

    echo ""
    echo "=== Upload complete ==="
    echo "Modal can pull from: r2://$R2_BUCKET/selfplay_20k_merged.jsonl.gz"
}

case "${1:-}" in
    setup)   cmd_setup ;;
    start)   cmd_start ;;
    status)  cmd_status ;;
    collect) cmd_collect ;;
    upload)  cmd_upload ;;
    *)
        echo "Usage: $0 {setup|start|status|collect|upload}"
        echo ""
        echo "  setup   — Install Rust, clone repo, build binary on remote machines"
        echo "  start   — Start self-play on Contabo + Mac Mini (runs in background)"
        echo "  status  — Check progress on both machines"
        echo "  collect — Rsync Mac Mini data to Contabo hub, merge JSONL files"
        echo "  upload  — Compress and upload merged data to Cloudflare R2"
        exit 1
        ;;
esac
