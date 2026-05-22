# Journal Entry Format

This document describes the file format used for journal entries, where they live on the SD card, and how to write entries in another app and import them onto the device.

## File layout on the SD card

Entries are stored as plain Markdown files under `/journal/YYYY/MM/`:

```
/journal/
  2026/
    05/
      20260520-103000.md
      20260521-083015.md
    06/
      20260601-180000.md
```

### Filename convention

Each file is named after the timestamp when the entry was created:

```
YYYYMMDD-HHMMSS.md
```

For example, `20260520-103000.md` = 20 May 2026, 10:30:00.

Filenames are always in local time as reported by the device clock. The firmware sorts entries alphabetically, so the timestamp-based naming keeps them in chronological order automatically.

## File format

Every entry is a Markdown file with a YAML-style frontmatter block at the top:

```markdown
---
title: My Entry Title
date: 2026-05-20 10:30:00
tags: personal, reflection
---
# My Entry Title

Body text goes here. Markdown is supported.
```

### Frontmatter fields

| Field | Required | Format | Description |
|---|---|---|---|
| `title` | Yes | Plain text | Entry title shown in the browse list |
| `date` | Yes | `YYYY-MM-DD HH:MM:SS` | Creation timestamp (local time) |
| `tags` | No | Comma-separated | Optional labels, e.g. `personal, diary` |

Rules:
- The block **must** start with `---` on its own line (no leading spaces or BOM).
- The block **must** end with `---` on its own line.
- Each key–value pair uses `key: value` with exactly one space after the colon.
- Unknown keys are silently ignored, so extra frontmatter fields are safe to include.
- A file with no frontmatter block is treated as body-only (the full file content becomes the body, with an empty title and date).

### Body

Everything after the closing `---` line is the body. The firmware renders Markdown on-device using the **XJL** (Xteink Journal Language) parser. The supported syntax is described below; keeping individual lines short (≤ 80 characters) makes entries more readable on the e-paper display.

#### Standard Markdown subset

| Syntax | Rendering |
|---|---|
| `# Heading` | Inverted (white-on-black) full-width bar |
| `## Heading` | Text with underline separator |
| `### Heading` | 1-character indent |
| `- item` / `* item` / `+ item` | Bullet list (dash marker + indent) |
| `  - item` (2+ spaces or tab) | Nested bullet (deeper indent) |
| `1. item` | Ordered list (keeps `N.` prefix) |
| `> text` | Blockquote (2 px left bar) |
| ` ``` … ``` ` | Fenced code block (3 px left bar, literal content) |
| `~~~` … `~~~` | Alternative code fence |
| `---` / `***` / `___` | Horizontal rule |
| (blank line) | Vertical spacer |

Inline markers are **stripped** (markers removed, text kept):

| Marker | Meaning | Rendering hint |
|---|---|---|
| `**text**` | Bold | Whole line renders faux-bold |
| `*text*` / `_text_` | Italic | Stripped (no font variant available) |
| `~~text~~` | Strikethrough | Stripped |
| `==text==` | Highlight | Stripped |
| `` `text` `` | Inline code | Whole line rendered with border box |
| `[text](url)` | Link (shows link text) | Stripped to text |
| `![alt](url)` | Image (shows alt text) | Stripped to alt |

> **Note on inline formatting:** The display uses fixed-pitch bitmap fonts with no italic or colour support. Bold and inline-code formatting are rendered as whole-line hints (faux-bold double-draw for `**bold**`; 1 px border box for `` `code` ``).

#### GFM-style tables

Pipe-separated tables use the standard GitHub Flavored Markdown table syntax:

```markdown
| Column 1 | Column 2 | Column 3 |
|----------|----------|----------|
| Cell A   | Cell B   | Cell C   |
| Cell D   | Cell E   | Cell F   |
```

- The first `|`-prefixed row is the header (rendered inverted, white text on black).
- The `|---|---|` separator row draws a horizontal divider.
- All subsequent `|`-prefixed rows are data rows.
- Inline markers inside cells are stripped (e.g. `**bold**` → `bold`).
- **Display limits:** at scale 2 in portrait mode (~30 chars wide), 2–4 columns fit comfortably. Five or more columns will truncate cell text.

A blank line or any non-`|` line ends the table.

#### XJL Bullet Journal extensions

Tasks use the GitHub-style checkbox notation:

| Syntax | Rendering |
|---|---|
| `- [ ] text` | Open task (empty checkbox □) |
| `- [x] text` | Done task (filled checkbox ■ + strikethrough) |
| `- [X] text` | Done task (same as `[x]`) |
| `- [>] text` | Migrated — moved to a future log |
| `- [<] text` | Scheduled — moved from a future log |

Signifiers mark the type of a line:

| Syntax | Rendering |
|---|---|
| `! text` | Priority |
| `@ text` | Event |
| `? text` | Question / reflection |

#### XJL callout blocks

Callouts extend blockquote syntax with a `[!TYPE]` prefix:

```markdown
> [!NOTE] This is a general note.
> [!TIP] A helpful tip goes here.
> [!WARNING] Something to watch out for.
> [!IMPORTANT] This must not be missed.
```

Supported types: `NOTE`, `TIP`, `WARNING`, `IMPORTANT` (case-insensitive). Each renders with a 3 px left bar and a small inverted label badge. Unknown types fall back to a regular blockquote.

#### XJL definition lists

A line immediately followed by a `: definition` line becomes a definition list term:

```markdown
Term
: The definition of the term goes here.

Another term
: Its definition.
```

- The term line renders faux-bold.
- The definition line is indented with a 2 px left bar.
- A blank line separates definition list entries.
- Lines starting with `:` but not followed by a space (e.g. ``:nodef``) are treated as normal text.

#### XJL habit / data grid

A compact grid for habit tracking or similar binary data:

```markdown
::grid Habit | Mon | Tue | Wed | Thu | Fri
Sleep        | x   | x   | .   | x   | x
Water        | .   | x   | x   | x   | .
```

- `::grid` starts the grid and sets the header row with column labels.
- Data rows start with a label and use `|` to separate values.
- In a data cell, `x` or `X` or `1` renders as a **filled box** (■); `.` or `0` or an empty cell renders as an **empty box** (□); any other value is shown as truncated text.
- A blank line or a line without `|` ends the grid block.

#### XJL Theme System extensions

The Theme System (from the [Cortex podcast](https://www.relay.fm/cortex)) is a yearly-theme productivity framework built around a broad guiding word or phrase rather than a rigid numerical goal. These XJL extensions let you embed Theme System concepts directly in your journal entries.

**Yearly theme declaration**

```markdown
::theme Year of Health
```

Renders as a full-width inverted bar (white text on black) with a `THEME` badge, identical in visual weight to an H1 heading.

**Seasonal review header**

```markdown
::season Spring 2026
```

Renders as a full-width inverted bar with a `SEASON` badge. Use it to mark the start of a quarterly (seasonal) review section.

**Daily alignment rating**

```markdown
::rating 3
```

Renders an `N/5` label followed by five boxes — filled (■) for each point up to N, empty (□) for the rest. Valid values are 1–5; values outside this range are clamped automatically.

**Theme-aligned observation**

```markdown
~ stayed focused on the theme today
~ skipped gym — not very Year of Health
```

Uses the `~` signifier (tilde + space at the start of a line). Renders with a `~` glyph in the marker zone and the observation text to the right — the same layout as `!` (priority) and `@` (event) signifiers.

**Example daily entry using Theme System extensions**

```markdown
---
title: Wednesday check-in
date: 2026-05-20 21:00:00
---
::theme Year of Health
::rating 4

## Today
~ hit the gym for 45 minutes
~ ate well, plenty of water
! schedule blood test

## Reflections
How have I grown since ::season Spring 2026 began?
```

### Minimal valid entry

```markdown
---
title: Quick note
date: 2026-05-20 10:30:00
---
Just a quick thought.
```

### Entry with no frontmatter

If a file contains no frontmatter at all, the entire file is used as the body and the title falls back to the human-readable label derived from the filename (`2026-05-20 10:30`).

---

## Writing entries in another app and importing them

There are three ways to get externally-written entries onto the device.

### Option 1 — Copy files directly onto the SD card

1. Power off the device and remove the SD card.
2. Create the directory structure `/journal/YYYY/MM/` for the month you want (e.g., `/journal/2026/05/`).
3. Place your `.md` files inside that directory following the `YYYYMMDD-HHMMSS.md` naming convention.
4. Reinsert the SD card and power on.

**Any Markdown editor** that can save plain `.md` files works for this method — Obsidian, Typora, iA Writer, VS Code, Notepad, etc.

> **Tip:** If the exact creation time doesn't matter, pick any timestamp that falls within the correct month and doesn't collide with an existing file. For example, if you wrote an entry on 3 June 2026, name it `20260603-120000.md`.

### Option 2 — Upload via the Web API (Wi-Fi)

When the device is on Wi-Fi (or acting as its own soft-AP `eJournal`), you can push entries over HTTP without removing the SD card.

#### Create a new entry (device picks the filename)

```bash
curl -X POST http://192.168.4.1/api/journal/new \
  -H "Content-Type: application/json" \
  -d '{"title":"My imported entry"}'
# → 201  {"path":"/journal/2026/05/20260520-103000.md"}
```

Then immediately save content to the returned path:

```bash
curl -X POST http://192.168.4.1/api/journal/entry \
  -H "Content-Type: application/json" \
  -d '{
    "path": "/journal/2026/05/20260520-103000.md",
    "content": "---\ntitle: My imported entry\ndate: 2026-05-20 10:30:00\ntags: imported\n---\nBody text here."
  }'
# → 200  ok
```

#### Save to a specific path (if you already know the filename)

```bash
curl -X POST http://192.168.4.1/api/journal/entry \
  -H "Content-Type: application/json" \
  -d '{
    "path": "/journal/2026/05/20260501-090000.md",
    "content": "---\ntitle: Backdated entry\ndate: 2026-05-01 09:00:00\n---\nWrote this on my laptop."
  }'
```

The `content` field must be the complete file contents — frontmatter block plus body — as a JSON string with `\n` for newlines.

#### Read an entry back

```bash
curl "http://192.168.4.1/api/journal/entry?path=/journal/2026/05/20260520-103000.md"
# → raw Markdown file content
```

#### List entries for a month

```bash
curl "http://192.168.4.1/api/journal/entries?year=2026&month=5"
# → JSON array: [{"path":"...","title":"...","date":"..."},...]
```

#### Export all entries at once

```bash
curl http://192.168.4.1/api/export/all
# → JSON array of all entries: [{"path":"...","title":"...","date":"...","tags":"...","body":"..."},...]
```

This is the quickest way to make a full backup or to bulk-import the journal into another tool.

### Option 3 — Edit in the browser SPA

Connect to the device's Wi-Fi network (or the `eJournal` soft-AP), then open `http://192.168.4.1` in a browser. The built-in web editor lets you create, edit, and delete entries without writing any code or curl commands.

---

## Encrypted entries (vault)

When the vault is unlocked, entries are transparently encrypted before being written to the SD card. An encrypted file looks like this:

```
---vault-v1---
<base64-encoded blob>
```

Encrypted entries **cannot** be imported by copying pre-written plaintext `.md` files — the device will only encrypt entries it creates or saves itself while the vault is unlocked.

If you want to import content into an encrypted vault:

1. Unlock the vault on the device (or via the API: `POST /api/vault/unlock {"pin":"1234"}`).
2. Use the Web API to `POST /api/journal/entry` with your plaintext Markdown content.
3. The firmware will encrypt the content automatically before writing to the SD card.

Vault encryption uses **AES-256-GCM** with a key derived from your PIN via PBKDF2-HMAC-SHA256 (10 000 iterations). The 16-byte salt is generated once and stored in the device's NVS (non-volatile storage) — it is not on the SD card, so copying encrypted files to a different device will not allow them to be decrypted.

> **Security note:** The PIN travels over plain HTTP when using the soft-AP. Do not reuse this PIN for other services while the soft-AP is reachable to untrusted devices.

---

## App compatibility

Because the format is plain Markdown with YAML frontmatter, entries are directly compatible with tools that understand frontmatter:

| App | Compatibility | Notes |
|---|---|---|
| **Obsidian** | ✅ Full | Import the `/journal/` folder as a vault; `title`, `date`, `tags` map to Obsidian properties |
| **iA Writer** | ✅ Full | Opens `.md` files natively; shows frontmatter as metadata |
| **Typora** | ✅ Full | Renders frontmatter as a YAML table |
| **VS Code** | ✅ Full | Any Markdown extension (e.g. Markdown All in One) reads the files as-is |
| **Logseq** | ⚠️ Partial | Import works; `date` field may not auto-link to the journal page |
| **Notion** | ⚠️ Partial | Use Notion's Markdown import; frontmatter becomes page properties |
| **Bear / Ulysses** | ⚠️ Partial | Import as plain Markdown; frontmatter is treated as text, not metadata |

For the `/api/export/all` JSON export, every standard JSON or spreadsheet tool can consume the output directly.
