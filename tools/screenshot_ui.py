#!/usr/bin/env python3
# tools/screenshot_ui.py — capture web UI screenshots without a physical device
#
# Starts the mock dev server (tools/serve_ui.py) internally, then drives a
# headless Chromium browser via Playwright to save PNG screenshots of each
# major UI view.
#
# Setup (one-time):
#   pip install playwright
#   python3 -m playwright install chromium
#
# Usage:
#   python3 tools/screenshot_ui.py                         # → docs/screenshots/
#   python3 tools/screenshot_ui.py --output-dir /tmp/shots
#   python3 tools/screenshot_ui.py --port 9292             # custom server port

import argparse
import sys
import threading
import time
from http.server import ThreadingHTTPServer
from pathlib import Path

_PROJECT_ROOT = Path(__file__).resolve().parent.parent

# Re-use the mock server logic from serve_ui.py
sys.path.insert(0, str(Path(__file__).parent))
try:
    from serve_ui import _seed_entries, build_handler  # type: ignore[import]
except ImportError as exc:
    raise SystemExit(f"Could not import serve_ui: {exc}") from exc

try:
    from playwright.sync_api import sync_playwright
except ImportError:
    raise SystemExit(
        "playwright is not installed.\n"
        "Run:  pip install playwright && python3 -m playwright install chromium"
    )


# ── server helpers ─────────────────────────────────────────────────────────────

def _start_server(port: int) -> ThreadingHTTPServer:
    """Start the mock dev server in a daemon thread."""
    store = _seed_entries()
    server = ThreadingHTTPServer(("127.0.0.1", port), build_handler(store))
    t = threading.Thread(target=server.serve_forever, daemon=True)
    t.start()
    # Give the server a moment to bind before Playwright connects.
    time.sleep(0.3)
    return server


# ── screenshot helpers ─────────────────────────────────────────────────────────

# Viewport matches a comfortable mobile-portrait reading width.
_VIEWPORT = {"width": 390, "height": 780}


def _save(page, path: Path) -> None:
    page.screenshot(path=str(path))
    try:
        display = path.relative_to(_PROJECT_ROOT)
    except ValueError:
        display = path
    print(f"  saved  {display}")


def capture_all(base_url: str, output_dir: Path) -> list:
    """Open a headless browser, walk through each UI view, save screenshots."""
    output_dir.mkdir(parents=True, exist_ok=True)
    saved = []

    with sync_playwright() as pw:
        browser = pw.chromium.launch(headless=True)
        page = browser.new_page(viewport=_VIEWPORT)

        # ── 1. Entry list (current month) ──────────────────────────────────────
        print("Capturing: entry list …")
        page.goto(base_url, wait_until="domcontentloaded")
        # Wait for entries to load — either items or the "No entries" message.
        page.wait_for_selector("#entry-list .entry-item, #entry-list p",
                               timeout=5000)
        out = output_dir / "web-ui-list.png"
        _save(page, out)
        saved.append(out)

        # ── 2. Entry editor ────────────────────────────────────────────────────
        print("Capturing: entry editor …")
        first = page.query_selector("#entry-list .entry-item")
        if first:
            first.click()
            # The editor panel gains the CSS class "visible" once loaded.
            page.wait_for_selector("#editor-panel.visible", timeout=5000)
            out = output_dir / "web-ui-editor.png"
            _save(page, out)
            saved.append(out)
            # Go back to the list.
            page.click("#back-btn")
            page.wait_for_selector("#list-panel", timeout=3000)
        else:
            print("  (no entries found — skipping editor view)")

        # ── 3. Empty month (navigate 3 months into the future) ─────────────────
        print("Capturing: empty month …")
        for _ in range(3):
            page.click("#next-btn")
        page.wait_for_selector(
            "#entry-list p",   # "No entries this month." paragraph
            timeout=5000,
        )
        out = output_dir / "web-ui-empty.png"
        _save(page, out)
        saved.append(out)

        browser.close()

    return saved


# ── CLI ────────────────────────────────────────────────────────────────────────

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Capture eJournal web UI screenshots using a headless browser. "
            "Starts the mock dev server internally — no device required."
        ),
    )
    parser.add_argument(
        "--output-dir",
        default=str(_PROJECT_ROOT / "docs" / "screenshots"),
        metavar="DIR",
        help="Directory to write PNG files into (default: docs/screenshots/)",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=9292,
        metavar="PORT",
        help="Port for the internal mock server (default: 9292)",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    output_dir = Path(args.output_dir)
    base_url = f"http://127.0.0.1:{args.port}"

    print(f"Starting mock server on {base_url} …")
    server = _start_server(args.port)

    try:
        print(f"Writing screenshots to {output_dir}/")
        saved = capture_all(base_url, output_dir)
    finally:
        server.shutdown()

    print(f"\nDone — {len(saved)} screenshot(s) saved.")


if __name__ == "__main__":
    main()
