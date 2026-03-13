#!/usr/bin/env bash
# Distributed self-play generation — one gzipped JSONL per seed for checkpointability.
# Runs selfplay_export with N parallel workers, skips seeds that already have output.
#
# Usage:
#   ./tools/distributed_selfplay.sh --seed-start 1 --seed-end 10000 --workers 14 --out-dir data/selfplay
#
# Resume: just re-run the same command — existing seed files are skipped.
# Merge:  zcat data/selfplay/seed_*.jsonl.gz > data/selfplay_merged.jsonl
#
# Storage: ~500KB per seed (gzipped from ~25MB raw). 20K seeds ≈ 10GB.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Defaults
SEED_START=1
SEED_END=10000
WORKERS=0  # 0 = auto-detect
OUT_DIR="$REPO_ROOT/data/selfplay"
MAX_TURNS=120
EXTRA_NODES=5000
LEAGUE=4
CONFIG_PATH="$REPO_ROOT/rust/bot/configs/submission_current.json"
BINARY=""
BUILD=1

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --seed-start N      First seed (default: 1)"
    echo "  --seed-end N        Last seed inclusive (default: 10000)"
    echo "  --workers N         Parallel workers, 0=auto (default: 0)"
    echo "  --out-dir PATH      Output directory (default: data/selfplay)"
    echo "  --max-turns N       Max turns per game (default: 120)"
    echo "  --extra-nodes N     Search budget (default: 5000)"
    echo "  --league N          League level (default: 4)"
    echo "  --config PATH       Bot config JSON (default: submission_current.json)"
    echo "  --binary PATH       Pre-built binary path (skip cargo build)"
    echo "  --no-build          Skip cargo build (use existing binary)"
    echo "  -h, --help          Show this help"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --seed-start) SEED_START="$2"; shift 2 ;;
        --seed-end) SEED_END="$2"; shift 2 ;;
        --workers) WORKERS="$2"; shift 2 ;;
        --out-dir) OUT_DIR="$2"; shift 2 ;;
        --max-turns) MAX_TURNS="$2"; shift 2 ;;
        --extra-nodes) EXTRA_NODES="$2"; shift 2 ;;
        --league) LEAGUE="$2"; shift 2 ;;
        --config) CONFIG_PATH="$2"; shift 2 ;;
        --binary) BINARY="$2"; shift 2 ;;
        --no-build) BUILD=0; shift ;;
        -h|--help) usage ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# Auto-detect workers
if [[ "$WORKERS" -eq 0 ]]; then
    if command -v nproc &>/dev/null; then
        WORKERS=$(( $(nproc) - 2 ))
    elif command -v sysctl &>/dev/null; then
        WORKERS=$(( $(sysctl -n hw.ncpu) - 2 ))
    else
        WORKERS=4
    fi
    [[ "$WORKERS" -lt 1 ]] && WORKERS=1
fi

# Build binary if needed
if [[ -z "$BINARY" ]]; then
    BINARY="$REPO_ROOT/target/release/selfplay_export"
    if [[ "$BUILD" -eq 1 ]]; then
        echo "Building selfplay_export (release)..."
        cargo build --release -p snakebot-bot --bin selfplay_export --manifest-path "$REPO_ROOT/Cargo.toml" 2>&1
        echo "Build complete."
    fi
fi

if [[ ! -x "$BINARY" ]]; then
    echo "ERROR: Binary not found or not executable: $BINARY"
    exit 1
fi

# Create output directory
mkdir -p "$OUT_DIR"

# Resolve git sha
GIT_SHA=$(cd "$REPO_ROOT" && git rev-parse HEAD 2>/dev/null || echo "unknown")

# Count how many seeds to process
TOTAL_SEEDS=$(( SEED_END - SEED_START + 1 ))
SKIPPED=0
PENDING_SEEDS=()

for (( seed=SEED_START; seed<=SEED_END; seed++ )); do
    SEED_GZ="$OUT_DIR/seed_$(printf '%05d' $seed).jsonl.gz"
    if [[ -f "$SEED_GZ" && -s "$SEED_GZ" ]]; then
        SKIPPED=$((SKIPPED + 1))
    else
        PENDING_SEEDS+=("$seed")
    fi
done

PENDING=${#PENDING_SEEDS[@]}
echo "Seeds: $TOTAL_SEEDS total, $SKIPPED already done, $PENDING to generate"
echo "Workers: $WORKERS | Max turns: $MAX_TURNS | Extra nodes: $EXTRA_NODES | League: $LEAGUE"
echo "Config: $CONFIG_PATH"
echo "Output: $OUT_DIR"
echo ""

if [[ "$PENDING" -eq 0 ]]; then
    echo "All seeds already generated. Nothing to do."
    TOTAL_FILES=$(ls -1 "$OUT_DIR"/seed_*.jsonl.gz 2>/dev/null | wc -l | tr -d ' ')
    echo "Total seed files: $TOTAL_FILES"
    exit 0
fi

echo "Starting generation (~500KB/seed compressed)..."
START_TIME=$(date +%s)

# Function to run one seed — generate, gzip, atomic rename
run_seed() {
    local seed=$1
    local seed_gz="$OUT_DIR/seed_$(printf '%05d' $seed).jsonl.gz"
    local tmp_file="${seed_gz%.gz}.tmp"

    "$BINARY" \
        --seed-start "$seed" \
        --seed-count 1 \
        --league "$LEAGUE" \
        --max-turns "$MAX_TURNS" \
        --extra-nodes-after-root "$EXTRA_NODES" \
        --config "$CONFIG_PATH" \
        --git-sha "$GIT_SHA" \
        --out "$tmp_file" 2>/dev/null

    # Compress and atomic rename
    gzip "$tmp_file"
    mv "${tmp_file}.gz" "$seed_gz"
}
export -f run_seed
export BINARY OUT_DIR LEAGUE MAX_TURNS EXTRA_NODES CONFIG_PATH GIT_SHA

# Run with parallel workers using xargs
printf '%s\n' "${PENDING_SEEDS[@]}" | xargs -P "$WORKERS" -I {} bash -c 'run_seed "$@"' _ {}

END_TIME=$(date +%s)
ELAPSED=$(( END_TIME - START_TIME ))
ELAPSED_MIN=$(echo "scale=1; $ELAPSED / 60" | bc)

# Final stats
TOTAL_FILES=$(ls -1 "$OUT_DIR"/seed_*.jsonl.gz 2>/dev/null | wc -l | tr -d ' ')
TOTAL_SIZE=$(du -sh "$OUT_DIR" 2>/dev/null | awk '{print $1}')

echo ""
echo "Done! Generated $PENDING seeds in ${ELAPSED_MIN} minutes"
echo "Total seed files: $TOTAL_FILES / $TOTAL_SEEDS"
echo "Total disk usage: $TOTAL_SIZE"
echo "Output: $OUT_DIR"
echo ""
echo "To merge: zcat $OUT_DIR/seed_*.jsonl.gz > data/selfplay_merged.jsonl"
echo "To upload to R2: zcat $OUT_DIR/seed_*.jsonl.gz | gzip > data/selfplay_merged.jsonl.gz && wrangler r2 object put snakebot-data/selfplay_20k_merged.jsonl.gz --file data/selfplay_merged.jsonl.gz"
