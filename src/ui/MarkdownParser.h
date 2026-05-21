#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// MarkdownParser.h — Subset Markdown → display-line converter
//
// Parses a journal body string into a flat list of word-wrapped MdLine
// structs that EntryScreen can render directly on the e-paper display.
//
// Supported syntax
// ────────────────
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
};

struct MdLine {
    MdLineType type;
    String     text;
    bool       continuation; // true = wrapped continuation of the same block
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
