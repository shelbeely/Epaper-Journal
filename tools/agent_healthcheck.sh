#!/usr/bin/env bash
# agent_healthcheck.sh — Poll /api/dev/health until pass or timeout
#
# Usage: ./agent_healthcheck.sh <device-ip> [timeout-seconds]
#
# Exits 0 on health-check pass, 1 on timeout or failure.
set -euo pipefail

HOST=${1:?"Usage: $0 <device-ip> [timeout-seconds]"}
TIMEOUT=${2:-30}

echo "Polling health: http://${HOST}/api/dev/health (timeout: ${TIMEOUT}s)"
END=$((SECONDS + TIMEOUT))

while [[ $SECONDS -lt $END ]]; do
    RESPONSE=$(curl -s --max-time 3 "http://${HOST}/api/dev/health" 2>/dev/null || true)
    if [[ -n "$RESPONSE" ]]; then
        OK=$(echo "$RESPONSE" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('ok','false'))" 2>/dev/null || echo "false")
        if [[ "$OK" == "True" || "$OK" == "true" ]]; then
            echo "Health check PASSED: $RESPONSE"
            exit 0
        else
            echo "Health check not yet OK: $RESPONSE"
        fi
    fi
    sleep 2
done

echo "Health check TIMED OUT after ${TIMEOUT}s" >&2
exit 1
