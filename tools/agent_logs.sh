#!/usr/bin/env bash
# agent_logs.sh — Stream or poll /api/dev/logs from the device
#
# Usage: ./agent_logs.sh <device-ip> [poll-interval-seconds]
#
# Continuously polls the log endpoint and pretty-prints new entries.
set -euo pipefail

HOST=${1:?"Usage: $0 <device-ip> [poll-interval-seconds]"}
INTERVAL=${2:-2}

echo "Streaming logs from http://${HOST}/api/dev/logs (interval: ${INTERVAL}s)"
echo "Press Ctrl+C to stop."

SEEN=""
while true; do
    RESPONSE=$(curl -s --max-time 3 "http://${HOST}/api/dev/logs" 2>/dev/null || true)
    if [[ -n "$RESPONSE" && "$RESPONSE" != "$SEEN" ]]; then
        echo "$RESPONSE" | python3 -c "
import sys, json
lines = json.load(sys.stdin)
for l in lines:
    print(l)
" 2>/dev/null || echo "$RESPONSE"
        SEEN="$RESPONSE"
    fi
    sleep "$INTERVAL"
done
