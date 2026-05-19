#!/usr/bin/env bash
# agent_build_dev.sh — Build the development firmware image
# Injects the current git commit hash as FIRMWARE_COMMIT build flag.
set -euo pipefail

COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
echo "Building dev firmware (commit: $COMMIT)..."

pio run -e dev \
    --project-option="build_flags=-DCONFIG_X4_DEV_DIAGNOSTICS=1 \
        -DCONFIG_X4_AGENT_DIAGNOSTICS=1 \
        -DCONFIG_X4_VERBOSE_DISPLAY_DIAGNOSTICS=1 \
        -DCONFIG_X4_DIAG_HTTP_API=1 \
        -DFIRMWARE_VERSION=\"0.0.0-dev\" \
        -DFIRMWARE_CHANNEL=\"dev\" \
        -DFIRMWARE_DEVICE=\"xteink-x4\" \
        -DFIRMWARE_COMMIT=\"${COMMIT}\""

echo "Build complete: .pio/build/dev/firmware.bin"
