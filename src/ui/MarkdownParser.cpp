// ─────────────────────────────────────────────────────────────────────────────
// MarkdownParser.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "MarkdownParser.h"

// ── stripInline ───────────────────────────────────────────────────────────────

String MarkdownParser::stripInline(const String& s) {
    String result;
    int i   = 0;
    int len = (int)s.length();

    while (i < len) {
        // ![alt](url) — images: emit alt text
        if (i + 1 < len && s[i] == '!' && s[i + 1] == '[') {
            int close = s.indexOf(']', i + 2);
            if (close >= 0 && close + 1 < len && s[close + 1] == '(') {
                int urlEnd = s.indexOf(')', close + 2);
                if (urlEnd >= 0) {
                    result += s.substring(i + 2, close);
                    i = urlEnd + 1;
                    continue;
                }
            }
        }

        // [text](url) — links: emit link text
        if (s[i] == '[') {
            int close = s.indexOf(']', i + 1);
            if (close >= 0 && close + 1 < len && s[close + 1] == '(') {
                int urlEnd = s.indexOf(')', close + 2);
                if (urlEnd >= 0) {
                    result += s.substring(i + 1, close);
                    i = urlEnd + 1;
                    continue;
                }
            }
        }

        // **bold** — check before single-asterisk
        if (i + 1 < len && s[i] == '*' && s[i + 1] == '*') {
            int close = s.indexOf("**", i + 2);
            if (close >= 0) {
                result += s.substring(i + 2, close);
                i = close + 2;
            } else {
                // No closing ** — emit both asterisks literally
                result += "**";
                i += 2;
            }
            continue;
        }

        // *italic*
        if (s[i] == '*') {
            int close = s.indexOf('*', i + 1);
            if (close >= 0) {
                result += s.substring(i + 1, close);
                i = close + 1;
                continue;
            }
        }

        // _italic_
        if (s[i] == '_') {
            int close = s.indexOf('_', i + 1);
            if (close >= 0) {
                result += s.substring(i + 1, close);
                i = close + 1;
                continue;
            }
        }

        // `code`
        if (s[i] == '`') {
            int close = s.indexOf('`', i + 1);
            if (close >= 0) {
                result += s.substring(i + 1, close);
                i = close + 1;
                continue;
            }
        }

        result += s[i];
        i++;
    }

    return result;
}

// ── _isHRule ──────────────────────────────────────────────────────────────────

bool MarkdownParser::_isHRule(const String& trimmed) {
    if (trimmed.length() < 3) return false;

    // Determine the rule character (first non-space char must be -, *, or _)
    char ruleChar = 0;
    int  count    = 0;
    for (int i = 0; i < (int)trimmed.length(); i++) {
        char c = trimmed[i];
        if (c == ' ') continue;
        if (ruleChar == 0) {
            if (c != '-' && c != '*' && c != '_') return false;
            ruleChar = c;
        } else if (c != ruleChar) {
            return false;
        }
        count++;
    }
    return count >= 3;
}

// ── _wrapAppend ───────────────────────────────────────────────────────────────

void MarkdownParser::_wrapAppend(std::vector<MdLine>& out,
                                  MdLineType type,
                                  const String& text,
                                  uint16_t firstMaxChars,
                                  uint16_t contMaxChars) {
    if (firstMaxChars == 0) firstMaxChars = 1;
    if (contMaxChars  == 0) contMaxChars  = 1;

    int  len        = (int)text.length();
    int  pos        = 0;
    bool firstLine  = true;

    while (pos < len) {
        uint16_t maxChars = firstLine ? firstMaxChars : contMaxChars;

        // Handle explicit newline (shouldn't appear inside a block line,
        // but guard defensively)
        if (text[pos] == '\n') {
            pos++;
            continue;
        }

        // Build one line up to maxChars, breaking at word boundaries
        int lineEnd   = pos;
        int lastSpace = -1;
        while (lineEnd < len && (lineEnd - pos) < (int)maxChars) {
            if (text[lineEnd] == '\n') break;
            if (text[lineEnd] == ' ') lastSpace = lineEnd;
            lineEnd++;
        }

        String lineText;
        if (lineEnd >= len || text[lineEnd] == '\n') {
            // Reached end or explicit newline — take everything up to here
            lineText = text.substring(pos, lineEnd);
            if (lineEnd < len && text[lineEnd] == '\n') lineEnd++;
            pos = lineEnd;
        } else if (lastSpace > pos) {
            // Break at last space within the line
            lineText = text.substring(pos, lastSpace);
            pos      = lastSpace + 1; // skip the space
        } else {
            // No space found — hard-break at max width
            lineText = text.substring(pos, lineEnd);
            pos      = lineEnd;
        }

        MdLine ml;
        ml.type         = type;
        ml.text         = lineText;
        ml.continuation = !firstLine;
        out.push_back(ml);
        firstLine = false;
    }

    // Emit at least one line even for empty text (e.g. "# " heading)
    if (firstLine) {
        MdLine ml;
        ml.type         = type;
        ml.text         = "";
        ml.continuation = false;
        out.push_back(ml);
    }
}

// ── parse ─────────────────────────────────────────────────────────────────────

std::vector<MdLine> MarkdownParser::parse(const String& body,
                                           uint16_t maxCharsNormal) {
    std::vector<MdLine> out;
    if (maxCharsNormal == 0) maxCharsNormal = 1;

    // Per-type character budgets (accounting for indent pixels at base scale).
    // These are conservative approximations based on typical display widths.
    // Bullet:     "- " drawn separately; text indented 2 chars → -2
    // Blockquote: left bar + 1-char gap → -1
    // Ordered:    "N. " in the text on the first line → same as normal;
    //             continuation indented 3 chars → -3
    const uint16_t maxH         = maxCharsNormal;
    const uint16_t maxBullet    = (maxCharsNormal > 2) ? maxCharsNormal - 2 : 1;
    const uint16_t maxBulletCont= maxBullet;
    const uint16_t maxOrdered   = maxCharsNormal;
    const uint16_t maxOrdCont   = (maxCharsNormal > 3) ? maxCharsNormal - 3 : 1;
    const uint16_t maxBQ        = (maxCharsNormal > 1) ? maxCharsNormal - 1 : 1;

    int  len = (int)body.length();
    int  i   = 0;

    while (i <= len) {
        // Extract one raw input line
        int nl = body.indexOf('\n', i);
        if (nl < 0) nl = len;

        String raw = body.substring(i, nl);
        i = nl + 1;

        // Trim trailing carriage return (Windows line endings)
        if (raw.length() > 0 && raw[raw.length() - 1] == '\r') {
            raw = raw.substring(0, raw.length() - 1);
        }

        // ── Blank line ────────────────────────────────────────────────────────
        if (raw.length() == 0) {
            MdLine ml;
            ml.type         = MD_BLANK;
            ml.text         = "";
            ml.continuation = false;
            out.push_back(ml);
            continue;
        }

        // ── Horizontal rule ───────────────────────────────────────────────────
        if (_isHRule(raw)) {
            MdLine ml;
            ml.type         = MD_HLINE;
            ml.text         = "";
            ml.continuation = false;
            out.push_back(ml);
            continue;
        }

        // ── ATX headings (# / ## / ###) ───────────────────────────────────────
        if (raw[0] == '#') {
            int level = 0;
            while (level < (int)raw.length() && raw[level] == '#') level++;
            // Require a space after the hashes to be a valid heading
            if (level < (int)raw.length() && raw[level] == ' ') {
                String text = stripInline(raw.substring(level + 1));
                MdLineType t = (level == 1) ? MD_H1 : (level == 2) ? MD_H2 : MD_H3;
                _wrapAppend(out, t, text, maxH, maxH);
                continue;
            }
        }

        // ── Unordered list: - / * / + (followed by a space) ──────────────────
        if (raw.length() >= 2 &&
            (raw[0] == '-' || raw[0] == '*' || raw[0] == '+') &&
            raw[1] == ' ') {
            String text = stripInline(raw.substring(2));
            _wrapAppend(out, MD_BULLET, text, maxBullet, maxBulletCont);
            continue;
        }

        // ── Ordered list: digits followed by ". " ─────────────────────────────
        {
            int j = 0;
            while (j < (int)raw.length() && raw[j] >= '0' && raw[j] <= '9') j++;
            if (j > 0 && j + 1 < (int)raw.length() &&
                raw[j] == '.' && raw[j + 1] == ' ') {
                // Keep the "N. " prefix in the first line's text
                String prefix  = raw.substring(0, j + 2); // "N. "
                String content = stripInline(raw.substring(j + 2));
                String firstText = prefix + content;
                _wrapAppend(out, MD_ORDERED, firstText, maxOrdered, maxOrdCont);
                continue;
            }
        }

        // ── Blockquote: > ─────────────────────────────────────────────────────
        if (raw.length() >= 2 && raw[0] == '>' && raw[1] == ' ') {
            String text = stripInline(raw.substring(2));
            _wrapAppend(out, MD_BLOCKQUOTE, text, maxBQ, maxBQ);
            continue;
        }
        if (raw.length() >= 1 && raw[0] == '>') {
            // ">" with no space — still a blockquote
            String text = stripInline(raw.substring(1));
            _wrapAppend(out, MD_BLOCKQUOTE, text, maxBQ, maxBQ);
            continue;
        }

        // ── Normal paragraph ──────────────────────────────────────────────────
        String text = stripInline(raw);
        _wrapAppend(out, MD_NORMAL, text, maxCharsNormal, maxCharsNormal);
    }

    return out;
}
