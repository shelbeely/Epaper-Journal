#!/usr/bin/env python3
# tools/screenshot_ui.py — capture web UI screenshots without a physical device
#
# Starts the mock dev server (tools/serve_ui.py) internally, then drives a
# headless Chromium browser via Playwright to save PNG screenshots of every
# meaningful UI state.
#
# Setup (one-time):
#   pip install playwright
#   python3 -m playwright install chromium
#
# Usage:
#   python3 tools/screenshot_ui.py                         # → docs/screenshots/
#   python3 tools/screenshot_ui.py --output-dir /tmp/shots
#   python3 tools/screenshot_ui.py --port 9292             # custom server port
#
# Output files
# ────────────────────────────────────────────────────────
#   list-current-month.png  Entry list — current month (many entries)
#   list-prev-month.png     Entry list — previous month
#   list-two-months-ago.png Entry list — two months ago
#   list-empty.png          Entry list — future month with no entries
#   editor-plain.png        Editor — plain prose entry
#   editor-rich.png         Editor — XJL-annotated entry (::theme etc.)
#   editor-long.png         Editor — long multi-section entry
#   editor-new.png          Editor — freshly created blank entry
#   editor-saved.png        Editor — "Saved." toast visible

import argparse
import datetime
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


def _load_list(page, base_url: str) -> None:
    """Navigate to the list panel and wait for it to settle."""
    page.goto(base_url, wait_until="domcontentloaded")
    page.wait_for_selector("#entry-list .entry-item, #entry-list p", timeout=5000)


def _open_entry_by_index(page, index: int) -> bool:
    """Click the nth entry in the list (0-based). Returns False if not found."""
    locator = page.locator("#entry-list .entry-item")
    count = locator.count()
    if index >= count:
        return False
    locator.nth(index).click()
    page.wait_for_selector("#editor-panel.visible", timeout=5000)
    return True


def _go_back(page) -> None:
    page.click("#back-btn")
    page.wait_for_selector("#entry-list .entry-item, #entry-list p", timeout=3000)


def _navigate_months(page, delta: int) -> None:
    """Click prev/next month button |delta| times (negative = prev).

    Uses expect_response to wait for each /api/journal/entries call to
    complete before returning, avoiding races with DOM re-render.
    """
    btn = "#prev-btn" if delta < 0 else "#next-btn"
    for _ in range(abs(delta)):
        with page.expect_response(
            lambda r: "/api/journal/entries" in r.url, timeout=5000
        ):
            page.click(btn)
        # The fetch response arrived; give the .then() callback a tick to
        # update innerHTML before we interact with the DOM.
        page.wait_for_timeout(150)
        page.wait_for_selector("#entry-list .entry-item, #entry-list p", timeout=5000)


def capture_all(base_url: str, output_dir: Path) -> list:
    """Open a headless browser, walk through each UI view, save screenshots."""
    output_dir.mkdir(parents=True, exist_ok=True)
    saved = []

    # Figure out which months carry entries based on today's date.
    today = datetime.date.today()
    cur_month  = today.month
    prev_month = (today.replace(day=1) - datetime.timedelta(days=1)).month

    with sync_playwright() as pw:
        browser = pw.chromium.launch(headless=True)
        page = browser.new_page(viewport=_VIEWPORT)

        # ── 1. List — current month (8 entries) ───────────────────────────────
        print("Capturing: list — current month …")
        _load_list(page, base_url)
        out = output_dir / "list-current-month.png"
        _save(page, out)
        saved.append(out)

        # ── 2. List — previous month (6 entries) ──────────────────────────────
        print("Capturing: list — previous month …")
        _navigate_months(page, -1)
        out = output_dir / "list-prev-month.png"
        _save(page, out)
        saved.append(out)

        # ── 3. List — two months ago (4 entries) ──────────────────────────────
        print("Capturing: list — two months ago …")
        _navigate_months(page, -1)
        out = output_dir / "list-two-months-ago.png"
        _save(page, out)
        saved.append(out)

        # ── 4. List — empty (4 months in the future) ──────────────────────────
        print("Capturing: list — empty month …")
        _navigate_months(page, +6)   # well past any seeded entries
        out = output_dir / "list-empty.png"
        _save(page, out)
        saved.append(out)

        # Return to current month for the editor shots.
        _load_list(page, base_url)

        # ── 5. Editor — plain prose entry (entry 0: "Morning Pages") ──────────
        print("Capturing: editor — plain entry …")
        if _open_entry_by_index(page, 0):
            out = output_dir / "editor-plain.png"
            _save(page, out)
            saved.append(out)
            _go_back(page)

        # ── 6. Editor — XJL-annotated entry (entry 1: "Project Reflection") ───
        print("Capturing: editor — XJL-annotated entry …")
        if _open_entry_by_index(page, 1):
            out = output_dir / "editor-rich.png"
            _save(page, out)
            saved.append(out)
            _go_back(page)

        # ── 7. Editor — long multi-section entry (entry 5: "System Design") ───
        print("Capturing: editor — long entry …")
        if _open_entry_by_index(page, 5):
            out = output_dir / "editor-long.png"
            _save(page, out)
            saved.append(out)
            # Show the "Saved." success toast while still in the editor.
            print("Capturing: editor — saved toast …")
            page.evaluate("showMsg('Saved.')")
            page.wait_for_selector("#msg", timeout=2000)
            out = output_dir / "editor-saved.png"
            _save(page, out)
            saved.append(out)
            _go_back(page)

        # ── 8. Editor — brand new blank entry ─────────────────────────────────
        print("Capturing: editor — new blank entry …")
        page.click("#new-btn")
        page.wait_for_selector("#editor-panel.visible", timeout=5000)
        out = output_dir / "editor-new.png"
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
