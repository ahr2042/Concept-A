#!/usr/bin/env bash
# Measure end-to-end frame rate out of the engine for one capture mode.
#
# Spawns the daemon on a scratch control socket, deploys one pipeline, then
# counts frames arriving on the video socket with fpsdisplaysink. Nothing is
# left running and the GUI is not involved, so the number is the engine's own
# throughput — see docs/PERFORMANCE.md for how to record a result.
#
#   scripts/bench-capture.sh <cap-index> [label] [algos-csv]
#
#   scripts/bench-capture.sh 0                      # cap 0, no processing
#   scripts/bench-capture.sh 1 "720p" canny         # cap 1 with the canny stage
#
# Run `printf 'create camera app t\ndevices 0\nquit\n' | ./concept_A/x64_debug/MediaFusionGCV`
# first to see which cap index is which.
set -uo pipefail

CAP="${1:?usage: bench-capture.sh <cap-index> [label] [algos-csv]}"
LABEL="${2:-cap $CAP}"
ALGOS="${3:-}"

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$REPO/concept_A/x64_debug/MediaFusionGCV"
CTL="/tmp/mediafusiongcv-bench-$$.sock"
LOG="$(mktemp -t mfg-bench-XXXXXX.log)"
SECONDS_TO_SAMPLE="${BENCH_SECONDS:-12}"

command -v socat      >/dev/null || { echo "need socat (apt install socat)"; exit 1; }
command -v gst-launch-1.0 >/dev/null || { echo "need gst-launch-1.0"; exit 1; }
[ -x "$BIN" ] || { echo "engine not built: $BIN"; exit 1; }

cleanup() { [ -n "${DPID:-}" ] && kill "$DPID" 2>/dev/null; wait "${DPID:-}" 2>/dev/null; rm -f "$CTL"; }
trap cleanup EXIT

rm -f "$CTL"
"$BIN" --serve "$CTL" >"$LOG" 2>&1 &
DPID=$!
for _ in $(seq 1 40); do [ -S "$CTL" ] && break; sleep 0.25; done
[ -S "$CTL" ] || { echo "daemon did not come up; see $LOG"; exit 1; }

send() { printf '%s\n' "$1" | timeout 20 socat -t5 - "UNIX-CONNECT:$CTL" 2>/dev/null | tr -d '\0'; }

send "create camera app bench" >/dev/null
send "set-device 0 0 $CAP"     >/dev/null
[ -n "$ALGOS" ] && send "algos 0 $ALGOS" >/dev/null
SOCK=$(send "start 0" | grep -o '/tmp/mediafusiongcv[^ ]*' | head -1)

echo "=== $LABEL (cap $CAP)${ALGOS:+ algos=$ALGOS} ==="
if [ -z "$SOCK" ]; then
    echo "  START FAILED — daemon log:"
    sed 's/^/    /' "$LOG"
    exit 1
fi

sleep 1   # let caps negotiation and exposure settle
timeout "$SECONDS_TO_SAMPLE" gst-launch-1.0 -v unixfdsrc socket-path="$SOCK" ! \
    fpsdisplaysink text-overlay=false video-sink=fakesink sync=false 2>&1 \
    | grep -oE 'rendered: [0-9]+, dropped: [0-9]+, current: [0-9.]+, average: [0-9.]+' \
    | tail -1 | sed 's/^/  /'
grep -E '^(capture|accel|detector|opencv):' "$LOG" | sed 's/^/  /'
