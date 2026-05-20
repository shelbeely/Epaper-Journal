#!/usr/bin/env python3
# ─────────────────────────────────────────────────────────────────────────────
# tools/embed_ui.py — gzip-compress data/index.html → src/web/ui_bundle.h
#
# PlatformIO pre-script usage (added to platformio.ini):
#   extra_scripts = pre:tools/embed_ui.py
#
# Standalone usage (regenerate header manually):
#   python3 tools/embed_ui.py
# ─────────────────────────────────────────────────────────────────────────────

import gzip
import os
import sys

# ── Detect whether we are running inside SCons (PlatformIO build) ─────────────
_is_scons = False
try:
    Import("env")  # SCons: inject the PlatformIO environment object
    _project_dir = env.subst("$PROJECT_DIR")  # type: ignore[name-defined]
    _is_scons = True
except NameError:
    # Running standalone — project root is one level above this script.
    _project_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SRC_HTML = os.path.join(_project_dir, "data", "index.html")
DST_H    = os.path.join(_project_dir, "src", "web", "ui_bundle.h")


def _generate(source=None, target=None, **kwargs):  # SCons signature
    """Compress data/index.html and write src/web/ui_bundle.h."""
    with open(SRC_HTML, "rb") as fh:
        html = fh.read()

    gz = gzip.compress(html, compresslevel=9)

    rows = [gz[i:i + 16] for i in range(0, len(gz), 16)]
    lines = [
        "// Auto-generated from data/index.html via tools/embed_ui.py.",
        "// Do not edit by hand — run:  python3 tools/embed_ui.py",
        "#pragma once",
        "#include <stddef.h>",
        "",
        "static const uint8_t UI_HTML_GZ[] = {",
    ]
    for row in rows:
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in row) + ",")
    lines.append("};")
    lines.append(f"static const size_t UI_HTML_GZ_LEN = {len(gz)};")
    lines.append("")

    with open(DST_H, "w") as fh:
        fh.write("\n".join(lines))

    print(
        f"embed_ui: {SRC_HTML} ({len(html)} B) "
        f"→ {DST_H} ({len(gz)} B gzip)"
    )


if _is_scons:
    # Regenerate the header before WebApi.cpp is compiled.
    env.AddPreAction(  # type: ignore[name-defined]
        "$BUILD_DIR/src/web/WebApi.cpp.o",
        _generate,
    )
else:
    _generate()
