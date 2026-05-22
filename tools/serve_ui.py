#!/usr/bin/env python3
# tools/serve_ui.py — off-device dev server for data/index.html
#
# Serves the eJournal web UI at http://localhost:8080 backed by an in-memory
# mock journal so you can develop and screenshot the UI without a physical
# device.  Edits made in the browser are kept in memory for the lifetime of
# the server process; they are not persisted to disk.
#
# Usage:
#   python3 tools/serve_ui.py [--port 8080]
#
# Then open  http://localhost:8080  in your browser.

import argparse
import datetime
import io
import json
import zipfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

_PROJECT_ROOT = Path(__file__).resolve().parent.parent
_HTML_PATH = _PROJECT_ROOT / "data" / "index.html"


def _seed_entries() -> dict:
    """Return a dict of path → raw Markdown content for sample journal entries."""
    today = datetime.date.today()

    def _entry(offset_days: int, hour: int, title: str, body: str) -> tuple:
        d = today - datetime.timedelta(days=offset_days)
        return d, hour, title, body

    samples = [
        # ── current month ──────────────────────────────────────────────────────
        _entry(0, 8, "Morning Pages",
               "Woke up before the alarm again. Sat with coffee by the window\n"
               "and wrote until the light changed.\n\n"
               "The day felt wider for it."),
        _entry(1, 21, "Project Reflection",
               "Spent the afternoon reviewing the firmware architecture.\n\n"
               "::theme: building\n"
               "::season: spring\n"
               "::rating: 4\n"
               "~ Feels satisfying when the pieces finally click into place.\n\n"
               "The display driver abstraction is cleaner than expected. The\n"
               "single-buffer constraint forced a simpler design."),
        _entry(3, 10, "Weekend Walk",
               "Long walk through the park. Left the phone at home for once.\n\n"
               "The light through the leaves was exactly right. A heron stood\n"
               "motionless at the edge of the pond for a full ten minutes."),
        _entry(5, 14, "Coffee Shop Notes",
               "## Observations\n\n"
               "The couple at the corner table argued in whispers for an hour\n"
               "then laughed about something and ordered cake.\n\n"
               "## Ideas\n\n"
               "- Sleep tracking via wrist sensor\n"
               "- Journal prompts seeded by season\n"
               "- Offline-first sync over BLE"),
        _entry(7, 9, "Reading Log — April",
               "Finished *The Dispossessed* last night. The parallel narrative\n"
               "structure lands harder on a second read.\n\n"
               "Now starting *A Pattern Language*. Already flagged twelve pages.\n\n"
               "::theme: reading\n"
               "::season: spring\n"
               "::rating: 5\n"
               "~ Some books rearrange the furniture in your mind."),
        _entry(10, 19, "System Design Draft",
               "## Context\n\n"
               "Mapping out the event loop for the next firmware revision.\n\n"
               "## Constraints\n\n"
               "- 320 KB DRAM total; ~180 KB available after stack and heap\n"
               "- Framebuffer must stay in single-buffer mode\n"
               "- No blocking calls in the WiFi task\n\n"
               "## Proposed approach\n\n"
               "Move all UI rendering to a dedicated FreeRTOS task pinned to\n"
               "core 0. Web API remains on core 1 via AsyncTCP."),
        _entry(12, 22, "Letters Unsent",
               "There are things I've been meaning to say for months.\n\n"
               "Not because they're hard — they're not, actually — but because\n"
               "putting them into words makes them real in a way that costs\n"
               "something.\n\n"
               "I don't know why I keep delaying. The cost of silence is higher.\n\n"
               "::theme: relationships\n"
               "::season: spring\n"
               "::rating: 3\n"
               "~ Worth finishing before the month is out."),
        _entry(14, 7, "Quick Thought",
               "Gratitude is a practice, not a feeling.\n\n"
               "You don't wait to feel it. You do the thing and it follows."),

        # ── previous month ─────────────────────────────────────────────────────
        _entry(32, 9, "April Check-in",
               "Reviewing the goals I set last month.\n\n"
               "Progress is slower than planned, but the direction feels right.\n"
               "The important thing is I haven't stopped."),
        _entry(35, 15, "Spring Plans",
               "Long list of things to build, places to see, books to read.\n\n"
               "Narrowed it down to three. Everything else is a maybe.\n\n"
               "::theme: planning\n"
               "::season: spring\n"
               "::rating: 3\n"
               "~ Constraints are a form of generosity."),
        _entry(38, 20, "Rainy Day",
               "Stayed in all day. Made soup from what was left in the fridge.\n\n"
               "Read half a novel. Napped. Watched the rain move across the\n"
               "window in sheets.\n\n"
               "Not a wasted day."),
        _entry(41, 11, "Book Finished: Piranesi",
               "Stayed up until 2am to finish it. Worth it.\n\n"
               "::theme: reading\n"
               "::season: spring\n"
               "::rating: 5\n"
               "~ The ending is exactly right and I am still thinking about it."),
        _entry(44, 16, "Travel Notes — Day 1",
               "## Getting there\n\n"
               "Train delayed by forty minutes. Met a retired cartographer in\n"
               "the waiting area who described mapping coastlines by hand.\n\n"
               "## Hotel\n\n"
               "Small room, large window, acceptable coffee. The city sounds\n"
               "different at this end."),
        _entry(47, 8, "Monthly Review — March",
               "## What shipped\n\n"
               "- Vault encryption layer\n"
               "- OTA update workflow\n"
               "- BMP screenshot API\n\n"
               "## What slipped\n\n"
               "- Theme screen polish\n"
               "- Battery indicator\n\n"
               "More of the former than I expected. Good month."),

        # ── two months ago ─────────────────────────────────────────────────────
        _entry(62, 9, "Winter Wrap-up",
               "The last cold morning. Or at least, the last one I'll call\n"
               "winter.\n\n"
               "::theme: transitions\n"
               "::season: winter\n"
               "::rating: 4\n"
               "~ Endings are just beginnings with better lighting."),
        _entry(65, 14, "Goals for Spring",
               "## Creative\n\n"
               "Finish the font renderer. Write more.\n\n"
               "## Physical\n\n"
               "Walk every morning before opening the laptop.\n\n"
               "## Relationships\n\n"
               "Call the people I've been meaning to call."),
        _entry(70, 19, "Code Review Thoughts",
               "Reviewed three PRs today. Good code. Clear naming. Tests that\n"
               "explain what they're testing.\n\n"
               "The best reviews aren't about catching mistakes — they're about\n"
               "understanding what the author was trying to do."),
        _entry(75, 11, "Museum Visit",
               "The textile collection on the third floor. Fabric from three\n"
               "centuries ago, still vivid.\n\n"
               "The placard said the dyes were plant-based. Indigo from a plant\n"
               "that no longer grows in that region.\n\n"
               "::theme: art\n"
               "::season: winter\n"
               "::rating: 4"),
    ]

    entries: dict = {}
    for date, hour, title, body in samples:
        ts = date.strftime("%Y%m%d") + f"-{hour:02d}0000"
        path = f"/journal/{date.year}/{date.month:02d}/{ts}.md"
        date_str = date.strftime("%Y-%m-%d") + f" {hour:02d}:00:00"
        entries[path] = f"---\ntitle: {title}\ndate: {date_str}\n---\n{body}"
    return entries


def _parse_frontmatter(content: str) -> tuple:
    """Extract (title, date) from YAML frontmatter, matching the JS logic."""
    title, date = "", ""
    if not content.startswith("---\n"):
        return title, date
    end = content.find("\n---\n", 4)
    if end < 0:
        return title, date
    for line in content[4:end].splitlines():
        if ": " in line:
            k, v = line.split(": ", 1)
            if k == "title":
                title = v
            elif k == "date":
                date = v
    return title, date


def build_handler(store: dict):
    class Handler(BaseHTTPRequestHandler):
        # ── helpers ────────────────────────────────────────────────────────────

        def _send_json(self, obj, status: int = 200) -> None:
            body = json.dumps(obj).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def _send_text(self, text, status: int = 200, content_type: str = "text/plain") -> None:
            body = text.encode("utf-8") if isinstance(text, str) else text
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def _send_bytes(self, data: bytes, status: int, content_type: str, extra_headers=()) -> None:
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            for name, value in extra_headers:
                self.send_header(name, value)
            self.end_headers()
            self.wfile.write(data)

        def _read_body(self) -> bytes:
            length = int(self.headers.get("Content-Length", 0))
            return self.rfile.read(length) if length > 0 else b""

        # ── GET ────────────────────────────────────────────────────────────────

        def do_GET(self):  # noqa: N802
            parsed = urlparse(self.path)
            path = parsed.path
            qs = parse_qs(parsed.query)

            # ── root — serve index.html ────────────────────────────────────────
            if path in ("/", "/index.html"):
                body = _HTML_PATH.read_bytes()
                self._send_bytes(body, 200, "text/html; charset=utf-8")
                return

            # ── PWA manifest ───────────────────────────────────────────────────
            if path == "/manifest.json":
                self._send_json({
                    "name": "eJournal",
                    "short_name": "eJournal",
                    "start_url": "/",
                    "display": "standalone",
                    "background_color": "#f5f0e8",
                    "theme_color": "#4a4a4a",
                })
                return

            # ── service worker stub (not needed in dev) ────────────────────────
            if path == "/sw.js":
                self._send_text("// service worker disabled in dev mode",
                                content_type="application/javascript")
                return

            # ── GET /api/journal/entries?year=YYYY&month=MM ────────────────────
            if path == "/api/journal/entries":
                year = int(qs.get("year", [0])[0])
                month = int(qs.get("month", [0])[0])
                result = []
                for p, content in store.items():
                    parts = p.split("/")       # ['', 'journal', 'YYYY', 'MM', ...]
                    if len(parts) >= 4:
                        try:
                            py, pm = int(parts[2]), int(parts[3])
                        except ValueError:
                            continue
                        if py == year and pm == month:
                            title, date = _parse_frontmatter(content)
                            result.append({"path": p, "title": title or "Untitled", "date": date})
                result.sort(key=lambda e: e.get("date", ""), reverse=True)
                self._send_json(result)
                return

            # ── GET /api/journal/entry?path=... ────────────────────────────────
            if path == "/api/journal/entry":
                entry_path = qs.get("path", [""])[0]
                if not entry_path:
                    self._send_text("missing path", 400)
                    return
                content = store.get(entry_path)
                if content is None:
                    self._send_text("not found", 404)
                    return
                self._send_text(content, content_type="text/markdown")
                return

            # ── GET /api/journal/search?q=<keyword> ───────────────────────────
            if path == "/api/journal/search":
                q = qs.get("q", [""])[0].strip().lower()
                if not q:
                    self._send_text("missing q", 400)
                    return
                result = []
                for p, content in store.items():
                    if q in content.lower():
                        title, date = _parse_frontmatter(content)
                        result.append({"path": p, "title": title or "Untitled", "date": date})
                self._send_json(result)
                return

            # ── GET /api/journal/export ────────────────────────────────────────
            if path == "/api/journal/export":
                buf = io.BytesIO()
                with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as zf:
                    for p, content in store.items():
                        zf.writestr(p.lstrip("/"), content)
                data = buf.getvalue()
                self._send_bytes(
                    data, 200, "application/zip",
                    extra_headers=[("Content-Disposition", 'attachment; filename="journal-backup.zip"')],
                )
                return

            self.send_response(404)
            self.end_headers()

        # ── POST ───────────────────────────────────────────────────────────────

        def do_POST(self):  # noqa: N802
            parsed = urlparse(self.path)
            path = parsed.path

            # ── POST /api/journal/new ─────────────────────────────────────────
            if path == "/api/journal/new":
                body_bytes = self._read_body()
                title = "New Entry"
                if body_bytes:
                    try:
                        obj = json.loads(body_bytes)
                        if obj.get("title"):
                            title = obj["title"]
                    except json.JSONDecodeError:
                        pass
                now = datetime.datetime.now()
                ts = now.strftime("%Y%m%d-%H%M%S")
                entry_path = f"/journal/{now.year}/{now.month:02d}/{ts}.md"
                date_str = now.strftime("%Y-%m-%d %H:%M:%S")
                store[entry_path] = f"---\ntitle: {title}\ndate: {date_str}\n---\n"
                self._send_json({"path": entry_path}, status=201)
                return

            # ── POST /api/journal/entry  body: {"path":"...","content":"..."} ──
            if path == "/api/journal/entry":
                body_bytes = self._read_body()
                try:
                    obj = json.loads(body_bytes)
                except json.JSONDecodeError:
                    self._send_text("bad json", 400)
                    return
                entry_path = obj.get("path", "")
                content = obj.get("content", "")
                if not entry_path or not content:
                    self._send_text("missing path or content", 400)
                    return
                store[entry_path] = content
                self._send_text("ok")
                return

            self.send_response(404)
            self.end_headers()

        # ── DELETE ─────────────────────────────────────────────────────────────

        def do_DELETE(self):  # noqa: N802
            parsed = urlparse(self.path)
            path = parsed.path
            qs = parse_qs(parsed.query)

            # ── DELETE /api/journal/entry?path=... ────────────────────────────
            if path == "/api/journal/entry":
                entry_path = qs.get("path", [""])[0]
                if not entry_path:
                    self._send_text("missing path", 400)
                    return
                if entry_path in store:
                    del store[entry_path]
                    self._send_text("ok")
                else:
                    self._send_text("not found", 404)
                return

            self.send_response(404)
            self.end_headers()

        def log_message(self, fmt, *args):  # noqa: A003
            print(f"{self.address_string()} - {fmt % args}")

    return Handler


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Off-device dev server for the eJournal web UI. "
            "Serves data/index.html with a seeded in-memory journal store."
        ),
    )
    parser.add_argument(
        "--port",
        type=int,
        default=8080,
        metavar="PORT",
        help="TCP port to listen on (default: 8080)",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if not _HTML_PATH.is_file():
        raise SystemExit(f"UI source not found: {_HTML_PATH}")

    store = _seed_entries()
    server = ThreadingHTTPServer(("127.0.0.1", args.port), build_handler(store))
    print(f"eJournal UI dev server — http://127.0.0.1:{args.port}/")
    print("Serving data/index.html with mock journal data. Ctrl-C to stop.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
