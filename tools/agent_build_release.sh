#!/usr/bin/env bash
# agent_build_release.sh — Build the release firmware image
# All CONFIG_X4_* diagnostic flags are OFF in the release environment.
set -euo pipefail

COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
VERSION=${RELEASE_VERSION:-"0.1.0"}

echo "Building release firmware v${VERSION} (commit: $COMMIT)..."

pio run -e release \
    --project-option="build_flags=-DCONFIG_X4_DEV_DIAGNOSTICS=0 \
        -DCONFIG_X4_AGENT_DIAGNOSTICS=0 \
        -DCONFIG_X4_VERBOSE_DISPLAY_DIAGNOSTICS=0 \
        -DCONFIG_X4_DIAG_HTTP_API=0 \
        -DFIRMWARE_VERSION=\"${VERSION}\" \
        -DFIRMWARE_CHANNEL=\"release\" \
        -DFIRMWARE_DEVICE=\"xteink-x4\" \
        -DFIRMWARE_COMMIT=\"${COMMIT}\""

echo "Build complete: .pio/build/release/firmware.bin"
