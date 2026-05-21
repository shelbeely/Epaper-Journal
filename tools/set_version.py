#!/usr/bin/env python3
"""Patch FIRMWARE_VERSION in platformio.ini.

Usage:
    python tools/set_version.py <semver>

Example:
    python tools/set_version.py 1.2.3
    python tools/set_version.py 1.2.3-rc.1

The script replaces every -DFIRMWARE_VERSION="..." occurrence in
platformio.ini so all build environments (dev, release, native) use the
same version string.  For dev/pre-release builds the caller should append
the appropriate pre-release suffix before invoking this script.
"""

import re
import sys
import pathlib


def main() -> None:
    if len(sys.argv) < 2:
        sys.exit("Usage: set_version.py <semver>")

    version = sys.argv[1]

    # Validate basic semver shape (MAJOR.MINOR.PATCH with optional pre-release).
    if not re.match(r"^\d+\.\d+\.\d+(-[A-Za-z0-9.]+)?$", version):
        sys.exit(f"Invalid semver string: {version!r}")

    ini_path = pathlib.Path("platformio.ini")
    if not ini_path.exists():
        sys.exit("platformio.ini not found — run from the repository root.")

    content = ini_path.read_text()

    # platformio.ini stores the flag as:  -DFIRMWARE_VERSION=\"<version>\"
    new_content, n = re.subn(
        r'-DFIRMWARE_VERSION=\\"[^\\"]*\\"',
        f'-DFIRMWARE_VERSION=\\"{version}\\"',
        content,
    )

    if n == 0:
        sys.exit("No -DFIRMWARE_VERSION pattern found in platformio.ini")

    ini_path.write_text(new_content)
    print(f"Updated {n} occurrence(s) of FIRMWARE_VERSION to {version!r}")


if __name__ == "__main__":
    main()
