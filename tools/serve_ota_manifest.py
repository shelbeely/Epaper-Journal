#!/usr/bin/env python3

import argparse
import hashlib
import json
import re
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Serve OTA manifest.json and firmware.bin on http://0.0.0.0:8080",
    )
    parser.add_argument("--bin", dest="bin_path", required=True, help="Path to firmware binary")
    parser.add_argument("--version", required=True, help="Firmware semver (e.g. 0.2.0)")
    args = parser.parse_args()

    bin_path = Path(args.bin_path)
    if not bin_path.is_file():
        parser.error(f"--bin file not found: {bin_path}")

    semver_pattern = r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$"
    if not re.match(semver_pattern, args.version):
        parser.error(f"--version must be semver, got: {args.version}")

    args.bin_path = bin_path.resolve()
    return args


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def build_handler(bin_path: Path, version: str, sha256_hex: str):
    firmware_bytes = bin_path.read_bytes()

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):  # noqa: N802
            path = urlparse(self.path).path
            if path == "/manifest.json":
                host_header = self.headers.get("Host", "localhost:8080")
                host = host_header.split(":", 1)[0] or "localhost"
                manifest = {
                    "version": version,
                    "url": f"http://{host}:8080/firmware.bin",
                    "sha256": sha256_hex,
                    "channel": "release",
                }
                body = json.dumps(manifest).encode("utf-8")
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
                return

            if path == "/firmware.bin":
                self.send_response(200)
                self.send_header("Content-Type", "application/octet-stream")
                self.send_header("Content-Length", str(len(firmware_bytes)))
                self.end_headers()
                self.wfile.write(firmware_bytes)
                return

            self.send_response(404)
            self.end_headers()

        def log_message(self, fmt, *args):  # noqa: A003
            print(f"{self.address_string()} - {fmt % args}")

    return Handler


def main() -> None:
    args = parse_args()
    sha256_hex = file_sha256(args.bin_path)
    print(f"Serving OTA files for {args.bin_path}")
    print(f"Version: {args.version}")
    print(f"SHA256 : {sha256_hex}")
    print("Manifest: http://0.0.0.0:8080/manifest.json")

    server = ThreadingHTTPServer(("0.0.0.0", 8080), build_handler(args.bin_path, args.version, sha256_hex))
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
