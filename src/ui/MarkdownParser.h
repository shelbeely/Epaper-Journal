#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// MarkdownParser.h — XJL (Xteink Journal Language) parser
//
// A Markdown subset extended with Bullet Journal signifiers, optimised for
// the Xteink X4 800×480 e-paper display (1-bit, 5×7 bitmap font).
//
// ── Standard Markdown subset ────────────────────────────────────────────────
//  # H1          → inverted (white-on-black) full-width bar
//  ## H2         → underline separator beneath the text
//  ### H3        → 1-char indent
//  - / * / + item → bullet list (dash prefix + 2-char indent)
//    (2+ leading spaces/tab) - / * / +  → nested bullet (4-char indent)
//  1. item        → ordered list (keeps "N." prefix, continuation indented)
//  > text         → blockquote (2 px left bar + 1-char indent)
//  ---  ***  ___  → horizontal rule (3+ identical chars, optional spaces)
//  ``` … ```      → fenced code block (3 px left bar, lines emitted verbatim)
//  ~~~ … ~~~      → fenced code block (alternative fence marker)
//  (blank line)   → empty spacer
//
//  Inline stripping (markers removed, inner text kept):
//    **bold**  *italic*  _italic_  ~~strikethrough~~  ==highlight==
//    `code`  [text](url)  ![alt](url)
//
//  Inline flags (set on the whole line, drive rendering hints):
//    **...**  → MdLine.bold = true   (faux-bold double-draw in renderer)
//    `...`    → MdLine.inlineCode = true  (border box around text)
//
// ── GFM-style tables ────────────────────────────────────────────────────────
//  | H1 | H2 |   → MD_TABLE_HEADER  (cells stored in MdLine.cells)
//  |----|----|   → MD_TABLE_SEP     (horizontal divider)
//  | D1 | D2 |   → MD_TABLE_ROW    (cells stored in MdLine.cells)
//
//  Practical display limits: 2–4 columns at scale 2 (portrait ~30 chars wide).
//
// ── XJL Bullet Journal extensions ───────────────────────────────────────────
//  Tasks — use the GitHub-style `- [x]` notation for easy web-editor entry:
//    - [ ] text  → open task     (empty checkbox □)
//    - [x] text  → done task     (filled checkbox ■ + strikethrough text)
//    - [X] text  → done task     (same as [x])
//    - [>] text  → migrated      (forward arrow >, text to right)
//    - [<] text  → scheduled     (back arrow <, text to right)
//
//  Signifiers — single punctuation character at the start of a line:
//    ! text      → priority      (! glyph, text to right)
//    @ text      → event         (@ glyph, text to right)
//    ? text      → question      (? glyph, text to right)
//
//  Marker zone: all task/signifier types reserve 2 char-widths (24 px at
//  scale 2) for the marker; text starts after that zone.
//
// ── XJL additional extensions ───────────────────────────────────────────────
//  Callout blocks (inside blockquote):
//    > [!NOTE] text      → highlighted note callout
//    > [!TIP] text       → tip callout
//    > [!WARNING] text   → warning callout
//    > [!IMPORTANT] text → important callout
//
//  Definition lists:
//    Term            → MD_DEFLIST_TERM  (faux-bold; detected via post-processing)
//    : Definition    → MD_DEFLIST_DEF   (indented, left bar)
//
//  Habit / data grid:
//    ::grid Label | Col1 | Col2   → MD_GRID_HEADER (first row)
//    Row label    | x    | .      → MD_GRID_ROW (x=filled, .=empty box)
//    (blank line or non-pipe exits grid mode)
//
// ── XJL Theme System extensions ──────────────────────────────────────────
//  Yearly theme declaration (inverted bar + THEME badge):
//    ::theme Year of Health        → MD_THEME
//
//  Seasonal review header (inverted bar + SEASON badge):
//    ::season Spring 2026          → MD_SEASON
//
//  Daily alignment rating (N/5 label + five filled/empty boxes):
//    ::rating 3                    → MD_RATING  (N clamped to 1–5)
//
//  Theme-aligned observation (tilde glyph in marker zone + text):
//    ~ text                        → MD_THEME_NOTE
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <vector>

enum MdLineType : uint8_t {
    MD_NORMAL = 0,
    MD_H1,
    MD_H2,
    MD_H3,
    MD_BULLET,
    MD_ORDERED,
    MD_BLOCKQUOTE,
    MD_HLINE,
    MD_BLANK,
    // ── XJL Bullet Journal extensions ────────────────────────────────────────
    MD_TASK_OPEN,       // - [ ] text  → open task (empty checkbox)
    MD_TASK_DONE,       // - [x] text  → done (filled checkbox + strikethrough)
    MD_TASK_MIGRATED,   // - [>] text  → migrated to future
    MD_TASK_SCHEDULED,  // - [<] text  → scheduled / moved back
    MD_PRIORITY,        // ! text      → priority signifier
    MD_EVENT,           // @ text      → event signifier
    MD_QUESTION,        // ? text      → question / reflection signifier
    // ── XJL Extended Markdown additions ──────────────────────────────────────
    MD_CODE_BLOCK,      // ``` … ``` / ~~~ … ~~~  fenced code block line
    MD_BULLET_NESTED,   // "  - text"  indented (nested) bullet item
    // ── GFM-style tables ─────────────────────────────────────────────────────
    MD_TABLE_HEADER,    // | H1 | H2 |  first row (inverted background)
    MD_TABLE_SEP,       // |----|----|  horizontal separator
    MD_TABLE_ROW,       // | D1 | D2 |  data row
    // ── XJL callout blocks ────────────────────────────────────────────────────
    MD_CALLOUT_NOTE,      // > [!NOTE] text
    MD_CALLOUT_TIP,       // > [!TIP] text
    MD_CALLOUT_WARNING,   // > [!WARNING] text
    MD_CALLOUT_IMPORTANT, // > [!IMPORTANT] text
    // ── XJL definition lists ─────────────────────────────────────────────────
    MD_DEFLIST_TERM,    // Term   (set by post-processing when followed by MD_DEFLIST_DEF)
    MD_DEFLIST_DEF,     // : Definition text
    // ── XJL habit / data grid ────────────────────────────────────────────────
    MD_GRID_HEADER,     // ::grid Label | Col1 | Col2 ...
    MD_GRID_ROW,        // Row label | x | . | x  (x=filled, .=empty cell)
    // ── XJL Theme System extensions ───────────────────────────────────────────
    MD_THEME,           // ::theme <text>   → inverted bar + THEME badge
    MD_SEASON,          // ::season <text>  → inverted bar + SEASON badge
    MD_RATING,          // ::rating N       → N/5 label + five filled/empty boxes (1–5)
    MD_THEME_NOTE,      // ~ text           → tilde glyph in marker zone + text
};

struct MdLine {
    MdLineType type         = MD_NORMAL;
    String     text;
    bool       continuation = false; // true = wrapped continuation of the same block
    bool       strikethrough = false; // true = draw strikethrough (MD_TASK_DONE)
    bool       bold         = false; // true = faux-bold rendering (whole line)
    bool       inlineCode   = false; // true = render with code-border box (whole line)
    std::vector<String> cells;       // table / grid cell contents (empty for non-table lines)
};

class MarkdownParser {
public:
    // Parse `body` and return a flat list of word-wrapped display lines.
    //
    // `maxCharsNormal` is the maximum number of characters that fit on one
    // line at the base scale with no indent (e.g. (dispW - 8) / charAdvPx).
    // Per-type char limits are derived from this value.
    static std::vector<MdLine> parse(const String& body,
                                     uint16_t maxCharsNormal);

    // Strip inline Markdown decorators from a single string.
    // Handles: **bold**  *italic*  _italic_  `code`  [text](url)  ![alt](url)
    static String stripInline(const String& s);

private:
    // Append word-wrapped lines of `type` into `out`.
    // `text` must already be inline-stripped.
    // `firstMaxChars` is the char budget for the first wrapped line;
    // `contMaxChars` is the budget for all subsequent continuation lines.
    // `bold` / `inlineCode` are propagated to every appended line.
    static void _wrapAppend(std::vector<MdLine>& out,
                            MdLineType type,
                            const String& text,
                            uint16_t firstMaxChars,
                            uint16_t contMaxChars,
                            bool bold = false,
                            bool inlineCode = false);

    // True if `raw` looks like a GFM table separator row (only |, -, :, spaces).
    static bool _isTableSep(const String& raw);

    // Split a GFM table / grid row into trimmed, inline-stripped cell strings.
    // Handles optional leading/trailing '|'.
    static std::vector<String> _splitCells(const String& row);

    // True if `line` (trimmed) looks like a horizontal rule:
    // 3+ identical characters from { '-', '*', '_' } optionally separated by spaces.
    static bool _isHRule(const String& trimmed);
};
