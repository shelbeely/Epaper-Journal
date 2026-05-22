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

        // ~~strikethrough~~ — strip markers, keep inner text
        if (i + 1 < len && s[i] == '~' && s[i + 1] == '~') {
            int close = s.indexOf("~~", i + 2);
            if (close >= 0) {
                result += s.substring(i + 2, close);
                i = close + 2;
            } else {
                result += "~~";
                i += 2;
            }
            continue;
        }

        // ==highlight== — strip markers, keep inner text
        if (i + 1 < len && s[i] == '=' && s[i + 1] == '=') {
            int close = s.indexOf("==", i + 2);
            if (close >= 0) {
                result += s.substring(i + 2, close);
                i = close + 2;
            } else {
                result += "==";
                i += 2;
            }
            continue;
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

// ── _isTableSep ───────────────────────────────────────────────────────────────

bool MarkdownParser::_isTableSep(const String& raw) {
    // Must start with '|' and contain only |, -, :, and spaces with at least one '-'
    if (raw.length() == 0 || raw[0] != '|') return false;
    bool hasDash = false;
    for (int i = 0; i < (int)raw.length(); i++) {
        char c = raw[i];
        if (c == '|' || c == ' ' || c == ':') continue;
        if (c == '-') { hasDash = true; continue; }
        return false; // unexpected character → not a separator
    }
    return hasDash;
}

// ── _splitCells ───────────────────────────────────────────────────────────────

std::vector<String> MarkdownParser::_splitCells(const String& row) {
    std::vector<String> cells;
    String s = row;
    s.trim();
    // Strip optional leading/trailing '|'
    if (s.length() > 0 && s[0] == '|') s = s.substring(1);
    if (s.length() > 0 && s[s.length() - 1] == '|') s = s.substring(0, s.length() - 1);

    int start = 0;
    int slen  = (int)s.length();
    for (int i = 0; i <= slen; i++) {
        if (i == slen || s[i] == '|') {
            String cell = s.substring(start, i);
            cell.trim();
            cells.push_back(stripInline(cell));
            start = i + 1;
        }
    }
    return cells;
}

// ── _wrapAppend ───────────────────────────────────────────────────────────────

void MarkdownParser::_wrapAppend(std::vector<MdLine>& out,
                                  MdLineType type,
                                  const String& text,
                                  uint16_t firstMaxChars,
                                  uint16_t contMaxChars,
                                  bool bold,
                                  bool inlineCode) {
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
        ml.bold         = bold;
        ml.inlineCode   = inlineCode;
        out.push_back(ml);
        firstLine = false;
    }

    // Emit at least one line even for empty text (e.g. "# " heading)
    if (firstLine) {
        MdLine ml;
        ml.type         = type;
        ml.text         = "";
        ml.continuation = false;
        ml.bold         = bold;
        ml.inlineCode   = inlineCode;
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
    // Bullet:       "- " drawn separately; text indented 2 chars → -2
    // Blockquote:   left bar + 1-char gap → -1
    // Ordered:      "N. " in the text on the first line → same as normal;
    //               continuation indented 3 chars → -3
    // Tasks:        marker zone = 2 char-widths → -2
    // Signifiers:   marker zone = 2 char-widths → -2
    const uint16_t maxH            = maxCharsNormal;
    const uint16_t maxBullet       = (maxCharsNormal > 2) ? maxCharsNormal - 2 : 1;
    const uint16_t maxBulletCont   = maxBullet;
    const uint16_t maxOrdered      = maxCharsNormal;
    const uint16_t maxOrdCont      = (maxCharsNormal > 3) ? maxCharsNormal - 3 : 1;
    const uint16_t maxBQ           = (maxCharsNormal > 1) ? maxCharsNormal - 1 : 1;
    const uint16_t maxCharsTask    = (maxCharsNormal > 2) ? maxCharsNormal - 2 : 1;
    const uint16_t maxCharsSignify = maxCharsTask;
    const uint16_t maxCodeBlock    = maxBQ;
    const uint16_t maxBulletNested = (maxCharsNormal > 4) ? maxCharsNormal - 4 : 1;

    int  len          = (int)body.length();
    int  i            = 0;
    bool inCodeFence  = false;
    bool inGrid       = false;    // inside ::grid block
    bool tableHeaderSeen = false; // first | row of a table has been emitted

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
            // Reset block-level state
            inGrid           = false;
            tableHeaderSeen  = false;
            if (inCodeFence) {
                // Preserve blank lines inside code fences as empty code lines
                MdLine ml;
                ml.type         = MD_CODE_BLOCK;
                ml.text         = "";
                ml.continuation = false;
                out.push_back(ml);
            } else {
                MdLine ml;
                ml.type         = MD_BLANK;
                ml.text         = "";
                ml.continuation = false;
                out.push_back(ml);
            }
            continue;
        }

        // ── Code fence toggle: ``` or ~~~ ────────────────────────────────────
        if (raw.length() >= 3 &&
            ((raw[0] == '`' && raw[1] == '`' && raw[2] == '`') ||
             (raw[0] == '~' && raw[1] == '~' && raw[2] == '~'))) {
            inCodeFence = !inCodeFence;
            continue;
        }

        // ── Inside code fence: emit lines literally (no inline stripping) ────
        if (inCodeFence) {
            _wrapAppend(out, MD_CODE_BLOCK, raw, maxCodeBlock, maxCodeBlock);
            continue;
        }

        // ── GFM table rows: lines starting with '|' ───────────────────────────
        if (raw.length() >= 1 && raw[0] == '|') {
            if (_isTableSep(raw)) {
                MdLine ml;
                ml.type         = MD_TABLE_SEP;
                ml.continuation = false;
                out.push_back(ml);
            } else {
                auto cells = _splitCells(raw);
                MdLineType t = tableHeaderSeen ? MD_TABLE_ROW : MD_TABLE_HEADER;
                MdLine ml;
                ml.type         = t;
                ml.continuation = false;
                ml.cells        = cells;
                out.push_back(ml);
                if (!tableHeaderSeen) {
                    tableHeaderSeen = true;
                }
            }
            continue;
        }
        // Reset table state when we leave a table block
        tableHeaderSeen = false;

        // ── Detect inline formatting flags (bold, inlineCode) on this raw line ─
        // These flags are applied to all wrapped lines produced from this input.
        bool lineBold     = false;
        bool lineCodeSpan = false;
        {
            int boldMarkers = 0;
            int codeMarkers = 0;
            int idx = 0;
            while ((idx = raw.indexOf("**", idx)) >= 0) {
                boldMarkers++;
                idx += 2;
            }
            for (int ci = 0; ci < (int)raw.length(); ci++) {
                if (raw[ci] == '`') codeMarkers++;
            }
            lineBold     = (boldMarkers >= 2);
            lineCodeSpan = (codeMarkers >= 2);
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
                _wrapAppend(out, t, text, maxH, maxH, lineBold, lineCodeSpan);
                continue;
            }
        }

        // ── XJL habit grid header: ::grid Label | Col1 | Col2 ────────────────
        if (raw.startsWith("::grid ")) {
            String afterPrefix = raw.substring(7); // after "::grid "
            afterPrefix.trim();
            auto cells = _splitCells(afterPrefix);
            MdLine ml;
            ml.type   = MD_GRID_HEADER;
            ml.cells  = cells;
            out.push_back(ml);
            inGrid = true;
            continue;
        }

        // ── XJL habit grid data row (inside grid block) ───────────────────────
        if (inGrid) {
            if (raw.indexOf('|') >= 0) {
                auto cells = _splitCells(raw);
                MdLine ml;
                ml.type  = MD_GRID_ROW;
                ml.cells = cells;
                out.push_back(ml);
                continue;
            } else {
                inGrid = false; // non-pipe line exits grid mode; fall through
            }
        }

        // ── XJL Bullet Journal tasks: - [ ]  - [x]  - [>]  - [<] ──────────────
        // Must be checked before the generic "- item" bullet rule because both
        // begin with "- ".  Pattern: - [<mark>] text  (length ≥ 6).
        if (raw.length() >= 6 &&
            raw[0] == '-' && raw[1] == ' ' &&
            raw[2] == '[' && raw[4] == ']' && raw[5] == ' ') {
            char mark = raw[3];
            MdLineType t;
            bool isDone = false;

            if (mark == ' ') {
                t = MD_TASK_OPEN;
            } else if (mark == 'x' || mark == 'X') {
                t      = MD_TASK_DONE;
                isDone = true;
            } else if (mark == '>') {
                t = MD_TASK_MIGRATED;
            } else if (mark == '<') {
                t = MD_TASK_SCHEDULED;
            } else {
                // Unknown mark → fall through to generic bullet below
                goto xjl_bullet_fallthrough;
            }

            {
                String text     = stripInline(raw.substring(6));
                int    prevSize = (int)out.size();
                _wrapAppend(out, t, text, maxCharsTask, maxCharsTask,
                            lineBold, lineCodeSpan);
                if (isDone) {
                    for (int k = prevSize; k < (int)out.size(); k++) {
                        out[k].strikethrough = true;
                    }
                }
            }
            continue;
        }
        xjl_bullet_fallthrough:

        // ── Indented (nested) bullet: 2+ spaces or tab then - / * / + ────────
        {
            int  indent = 0;
            bool hasTab = false;
            while (indent < (int)raw.length() &&
                   (raw[indent] == ' ' || raw[indent] == '\t')) {
                if (raw[indent] == '\t') hasTab = true;
                indent++;
            }
            if ((indent >= 2 || hasTab) &&
                indent + 1 < (int)raw.length() &&
                (raw[indent] == '-' || raw[indent] == '*' || raw[indent] == '+') &&
                raw[indent + 1] == ' ') {
                String text = stripInline(raw.substring(indent + 2));
                _wrapAppend(out, MD_BULLET_NESTED, text,
                            maxBulletNested, maxBulletNested, lineBold, lineCodeSpan);
                continue;
            }
        }

        // ── Unordered list: - / * / + (followed by a space) ──────────────────
        if (raw.length() >= 2 &&
            (raw[0] == '-' || raw[0] == '*' || raw[0] == '+') &&
            raw[1] == ' ') {
            String text = stripInline(raw.substring(2));
            _wrapAppend(out, MD_BULLET, text, maxBullet, maxBulletCont,
                        lineBold, lineCodeSpan);
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
                _wrapAppend(out, MD_ORDERED, firstText, maxOrdered, maxOrdCont,
                            lineBold, lineCodeSpan);
                continue;
            }
        }

        // ── Blockquote: > (with optional callout: > [!TYPE] text) ────────────
        if (raw.length() >= 2 && raw[0] == '>' && raw[1] == ' ') {
            String bqText = raw.substring(2);

            // ── Callout / admonition: > [!NOTE] / [!TIP] / [!WARNING] / [!IMPORTANT]
            if (bqText.startsWith("[!")) {
                int close = bqText.indexOf(']', 2);
                if (close > 2) {
                    // Build uppercase tag for case-insensitive comparison
                    String tag = bqText.substring(2, close);
                    String tagUp;
                    for (int ci = 0; ci < (int)tag.length(); ci++) {
                        tagUp += (char)toupper((unsigned char)tag[ci]);
                    }
                    MdLineType ct = MD_CALLOUT_NOTE;
                    bool known = true;
                    if      (tagUp == "NOTE")      ct = MD_CALLOUT_NOTE;
                    else if (tagUp == "TIP")       ct = MD_CALLOUT_TIP;
                    else if (tagUp == "WARNING")   ct = MD_CALLOUT_WARNING;
                    else if (tagUp == "IMPORTANT") ct = MD_CALLOUT_IMPORTANT;
                    else                           known = false;

                    if (known) {
                        String content = (close + 1 < (int)bqText.length())
                                         ? bqText.substring(close + 1) : "";
                        content.trim();                        _wrapAppend(out, ct, stripInline(content), maxBQ, maxBQ,
                                    lineBold, lineCodeSpan);
                        continue;
                    }
                }
            }

            // Regular blockquote
            _wrapAppend(out, MD_BLOCKQUOTE, stripInline(bqText), maxBQ, maxBQ,
                        lineBold, lineCodeSpan);
            continue;
        }
        if (raw.length() >= 1 && raw[0] == '>') {
            // ">" with no space — still a blockquote
            String text = stripInline(raw.substring(1));
            _wrapAppend(out, MD_BLOCKQUOTE, text, maxBQ, maxBQ,
                        lineBold, lineCodeSpan);
            continue;
        }

        // ── XJL signifiers: ! / @ / ? (at line start, followed by a space) ────
        if (raw.length() >= 2 && raw[1] == ' ') {
            MdLineType t = MD_NORMAL;
            if      (raw[0] == '!') t = MD_PRIORITY;
            else if (raw[0] == '@') t = MD_EVENT;
            else if (raw[0] == '?') t = MD_QUESTION;

            if (t != MD_NORMAL) {
                String text = stripInline(raw.substring(2));
                _wrapAppend(out, t, text, maxCharsSignify, maxCharsSignify,
                            lineBold, lineCodeSpan);
                continue;
            }
        }

        // ── Definition list: ": Definition" ──────────────────────────────────
        if (raw.length() >= 2 && raw[0] == ':' && raw[1] == ' ') {
            String text = stripInline(raw.substring(2));
            _wrapAppend(out, MD_DEFLIST_DEF, text, maxBQ, maxBQ,
                        lineBold, lineCodeSpan);
            continue;
        }

        // ── XJL Theme System: ::theme <text> ─────────────────────────────────
        if (raw.startsWith("::theme ")) {
            String text = stripInline(raw.substring(8));
            text.trim();
            _wrapAppend(out, MD_THEME, text, maxH, maxH);
            continue;
        }

        // ── XJL Theme System: ::season <text> ────────────────────────────────
        if (raw.startsWith("::season ")) {
            String text = stripInline(raw.substring(9));
            text.trim();
            _wrapAppend(out, MD_SEASON, text, maxH, maxH);
            continue;
        }

        // ── XJL Theme System: ::rating N ─────────────────────────────────────
        if (raw.startsWith("::rating ")) {
            String numStr = raw.substring(9);
            numStr.trim();
            int n = atoi(numStr.c_str());
            if (n < 1) n = 1;
            if (n > 5) n = 5;
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", n);
            MdLine ml;
            ml.type = MD_RATING;
            ml.text = String(buf);
            out.push_back(ml);
            continue;
        }

        // ── XJL Theme System: ~ text (theme note) ────────────────────────────
        if (raw.length() >= 2 && raw[0] == '~' && raw[1] == ' ') {
            String text = stripInline(raw.substring(2));
            _wrapAppend(out, MD_THEME_NOTE, text, maxCharsSignify, maxCharsSignify,
                        lineBold, lineCodeSpan);
            continue;
        }

        // ── Normal paragraph ──────────────────────────────────────────────────
        String text = stripInline(raw);
        _wrapAppend(out, MD_NORMAL, text, maxCharsNormal, maxCharsNormal,
                    lineBold, lineCodeSpan);
    }

    // ── Post-process: promote MD_NORMAL immediately before MD_DEFLIST_DEF ──────
    // A non-continuation normal line that is immediately followed by a
    // non-continuation def line becomes the definition list term.
    for (int k = 1; k < (int)out.size(); k++) {
        if (out[k].type == MD_DEFLIST_DEF && !out[k].continuation &&
            out[k - 1].type == MD_NORMAL  && !out[k - 1].continuation) {
            out[k - 1].type = MD_DEFLIST_TERM;
            out[k - 1].bold = true; // terms always render faux-bold
        }
    }

    return out;
}
