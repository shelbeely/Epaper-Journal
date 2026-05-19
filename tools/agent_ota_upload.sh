#!/usr/bin/env bash
# agent_ota_upload.sh — Upload a firmware binary to the device OTA endpoint
#
# Usage: ./agent_ota_upload.sh <device-ip> <firmware.bin>
#
# The device must be running with CONFIG_X4_DIAG_HTTP_API=1 and have
# Wi-Fi connectivity for this script to work.
set -euo pipefail

HOST=${1:?"Usage: $0 <device-ip> <firmware.bin>"}
BIN=${2:?"Usage: $0 <device-ip> <firmware.bin>"}

if [[ ! -f "$BIN" ]]; then
    echo "Error: firmware file not found: $BIN" >&2
    exit 1
fi

echo "Triggering OTA apply on $HOST..."
RESULT=$(curl -s -X POST "http://${HOST}/api/dev/ota/apply")
echo "OTA result: $RESULT"
