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
    samples = [
        (
            today,
            "Morning Pages",
            "Woke up early and wrote a few pages before breakfast.\n\n"
            "The day stretched out ahead, full of possibility.",
        ),
        (
            today - datetime.timedelta(days=1),
            "Project Reflection",
            "Spent the afternoon reviewing the firmware architecture.\n\n"
            "::theme: building\n"
            "::season: spring\n"
            "::rating: 4\n"
            "~ Feels satisfying when the pieces finally click into place.",
        ),
        (
            today - datetime.timedelta(days=3),
            "Weekend Walk",
            "Long walk through the park. Left the phone at home for once.\n\n"
            "The light through the leaves was exactly right.",
        ),
        (
            today - datetime.timedelta(days=30),
            "April Check-in",
            "Reviewing the goals I set last month. Progress is slower than\n"
            "planned, but the direction feels right.",
        ),
        (
            today - datetime.timedelta(days=35),
            "Spring Plans",
            "Long list of things to build, places to see, books to read.\n\n"
            "Narrowed it down to three.\n\n"
            "::theme: planning\n::season: spring\n::rating: 3",
        ),
        (
            today - datetime.timedelta(days=60),
            "Monthly Review",
            "Quarter is almost done. Looking back at what shipped and what\n"
            "slipped. More of the former than expected.",
        ),
    ]

    entries: dict = {}
    for date, title, body in samples:
        ts = date.strftime("%Y%m%d") + "-090000"
        path = f"/journal/{date.year}/{date.month:02d}/{ts}.md"
        date_str = date.strftime("%Y-%m-%d") + " 09:00:00"
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
