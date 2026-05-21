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
//  1. item        → ordered list (keeps "N." prefix, continuation indented)
//  > text         → blockquote (2 px left bar + 1-char indent)
//  ---  ***  ___  → horizontal rule (3+ identical chars, optional spaces)
//  (blank line)   → empty spacer
//
//  Inline stripping (markers removed, inner text kept):
//    **bold**  *italic*  _italic_  `code`  [text](url)  ![alt](url)
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
};

struct MdLine {
    MdLineType type;
    String     text;
    bool       continuation = false; // true = wrapped continuation of the same block
    bool       strikethrough = false; // true = draw strikethrough (MD_TASK_DONE)
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
    static void _wrapAppend(std::vector<MdLine>& out,
                            MdLineType type,
                            const String& text,
                            uint16_t firstMaxChars,
                            uint16_t contMaxChars);

    // True if `line` (trimmed) looks like a horizontal rule:
    // 3+ identical characters from { '-', '*', '_' } optionally separated by spaces.
    static bool _isHRule(const String& trimmed);
};
