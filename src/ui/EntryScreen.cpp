// ─────────────────────────────────────────────────────────────────────────────
// EntryScreen.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "EntryScreen.h"

EntryScreen::EntryScreen(X4Display& display, X4Input& input,
                         SleepScreen& sleepScreen)
    : _display(display), _input(input), _sleepScreen(sleepScreen),
      _titleH((uint16_t)(_display.lineHeight(SCALE) * 2)),
      _bodyY((uint16_t)(_titleH + 4)),
      _itemH(_display.lineHeight(SCALE))
{}

void EntryScreen::show(const JournalEntry& entry) {
    uint16_t dispW = _display.width();

    // Characters per line for body text at SCALE
    uint16_t maxChars = (dispW - 8) / _display.charAdvance(SCALE);

    std::vector<MdLine> lines = MarkdownParser::parse(entry.body, maxChars);
    const int totalLines  = (int)lines.size();
    const int linesPerPage = (_display.height() - _bodyY - 4) / _itemH;
    int topLine = 0;
    bool needRedraw = true;
    bool fullRefreshPending = true;

    while (true) {
        _input.tick();

        // Power-button sleep screen
        if (_input.isPowerButtonPressed()) {
            _sleepScreen.sleep(0); // does not return
        }

        if (_input.wasUp() || _input.wasLeft()) {
            if (topLine > 0) {
                topLine = (topLine > linesPerPage) ? topLine - linesPerPage : 0;
                needRedraw = true;
            }
        }
        if (_input.wasDown() || _input.wasRight()) {
            if (topLine + linesPerPage < totalLines) {
                topLine += linesPerPage;
                needRedraw = true;
            }
        }
        if (_input.wasBack()) {
            return;
        }

        if (needRedraw) {
            _renderPage(entry.title, lines, topLine, totalLines);
            _display.displayGrayscale();
            fullRefreshPending = false;
            needRedraw = false;
        }

        delay(10);
    }
}

// ── _drawLineText ─────────────────────────────────────────────────────────────

void EntryScreen::_drawLineText(uint16_t x, uint16_t y,
                                 const char* str, GrayLevel fg, GrayLevel bg,
                                 uint8_t scale, const MdLine& line) {
    // Inline-code border box: 1 px LIGHT_GRAY frame around the text glyph area.
    // Only drawn on a white background (not inside an inverted/dark bar).
    if (line.inlineCode && bg == GrayLevel::WHITE) {
        uint16_t tw = (uint16_t)(strlen(str) * _display.charAdvance(scale));
        uint16_t gh = _display.glyphHeight(scale);
        if (tw > 0) {
            _display.fillRectGray(x - 1, y,         tw + 2, 1,      GrayLevel::LIGHT_GRAY); // top
            _display.fillRectGray(x - 1, y + gh,     tw + 2, 1,      GrayLevel::LIGHT_GRAY); // bottom
            _display.fillRectGray(x - 1, y,         1,      gh + 1, GrayLevel::LIGHT_GRAY); // left
            _display.fillRectGray(x + tw, y,         1,      gh + 1, GrayLevel::LIGHT_GRAY); // right
        }
    }
    _display.drawTextGray(x, y, str, fg, bg, scale);
    // Faux-bold: draw again 1 px to the right.
    if (line.bold) {
        _display.drawTextGray(x + 1, y, str, fg, bg, scale);
    }
}

// ── _renderLine ───────────────────────────────────────────────────────────────

void EntryScreen::_renderLine(uint16_t dispW,
                               const MdLine& line, uint16_t y) {
    const uint16_t charAdv = _display.charAdvance(SCALE);
    const uint16_t margin  = 4;

    // ── XJL marker zone constants ─────────────────────────────────────────────
    // All task/signifier types share a 2-char-wide marker zone;
    // text starts at margin + 2*charAdv.
    const uint16_t textX_bj = margin + charAdv * 2;

    // Checkbox: 10×10 px, vertically centred in _itemH
    const uint16_t BOX_SZ  = (uint16_t)(SCALE * 5);
    const uint16_t BOX_OFF = (uint16_t)((_itemH - BOX_SZ) / 2);
    const uint16_t boxX    = margin;
    const uint16_t boxY    = y + BOX_OFF;

    // Glyph y for 1-char signifiers (centre glyph in _itemH)
    const uint16_t glyphY  = y + (uint16_t)((_itemH - _display.glyphHeight(SCALE)) / 2);

    switch (line.type) {

    case MD_H1: {
        // DARK_GRAY bar, WHITE text
        _display.fillRectGray(0, y, dispW, _itemH, GrayLevel::DARK_GRAY);
        _drawLineText(margin, y, line.text.c_str(), GrayLevel::WHITE, GrayLevel::DARK_GRAY, SCALE, line);
        break;
    }

    case MD_H2: {
        _drawLineText(margin, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        // DARK_GRAY underline 1 px below the glyph body
        uint16_t underY = y + _display.glyphHeight(SCALE) + 1;
        _display.fillRectGray(margin, underY, dispW - margin * 2, 1, GrayLevel::DARK_GRAY);
        break;
    }

    case MD_H3: {
        // 1-char indent
        _drawLineText(margin + charAdv, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        break;
    }

    case MD_BULLET: {
        if (!line.continuation) {
            _display.drawCharGray(margin, y, '-', GrayLevel::BLACK, GrayLevel::WHITE, SCALE);
        }
        _drawLineText(margin + charAdv * 2, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        break;
    }

    case MD_ORDERED: {
        if (!line.continuation) {
            _drawLineText(margin, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        } else {
            _drawLineText(margin + charAdv * 3, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        }
        break;
    }

    case MD_BLOCKQUOTE: {
        // DARK_GRAY 2 px left bar
        _display.fillRectGray(margin, y, 2, _itemH, GrayLevel::DARK_GRAY);
        _drawLineText(margin + charAdv + 4, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        break;
    }

    case MD_HLINE: {
        // DARK_GRAY centred 2 px horizontal rule
        uint16_t ruleY = y + _itemH / 2 - 1;
        _display.fillRectGray(margin, ruleY, dispW - margin * 2, 2, GrayLevel::DARK_GRAY);
        break;
    }

    // ── XJL task types ────────────────────────────────────────────────────────

    case MD_TASK_OPEN: {
        // Draw empty checkbox outline (BOX_SZ × BOX_SZ px)
        _display.fillRectGray(boxX,              boxY,              BOX_SZ, 1,      GrayLevel::BLACK); // top
        _display.fillRectGray(boxX,              boxY + BOX_SZ - 1, BOX_SZ, 1,      GrayLevel::BLACK); // bottom
        _display.fillRectGray(boxX,              boxY,              1,      BOX_SZ, GrayLevel::BLACK); // left
        _display.fillRectGray(boxX + BOX_SZ - 1, boxY,              1,      BOX_SZ, GrayLevel::BLACK); // right
        _drawLineText(textX_bj, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        break;
    }

    case MD_TASK_DONE: {
        // Checkbox border
        _display.fillRectGray(boxX,              boxY,              BOX_SZ, 1,      GrayLevel::BLACK); // top
        _display.fillRectGray(boxX,              boxY + BOX_SZ - 1, BOX_SZ, 1,      GrayLevel::BLACK); // bottom
        _display.fillRectGray(boxX,              boxY,              1,      BOX_SZ, GrayLevel::BLACK); // left
        _display.fillRectGray(boxX + BOX_SZ - 1, boxY,              1,      BOX_SZ, GrayLevel::BLACK); // right
        // DARK_GRAY fill (softer than solid black for completed state)
        _display.fillRectGray(boxX + 2, boxY + 2, BOX_SZ - 4, BOX_SZ - 4, GrayLevel::DARK_GRAY);
        _drawLineText(textX_bj, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        // Strikethrough: 1 px line at glyph vertical midpoint
        if (line.strikethrough) {
            uint16_t tLen    = (uint16_t)line.text.length();
            uint16_t tW      = (uint16_t)(tLen * charAdv);
            uint16_t maxW    = (dispW > textX_bj + 4u) ? dispW - textX_bj - 4u : 0u;
            if (tW > maxW) tW = maxW;
            uint16_t strikeY = y + _display.glyphHeight(SCALE) / 2;
            if (tW > 0) _display.fillRectGray(textX_bj, strikeY, tW, 1, GrayLevel::DARK_GRAY);
        }
        break;
    }

    case MD_TASK_MIGRATED: {
        _display.drawCharGray(margin + 2, glyphY, '>', GrayLevel::BLACK, GrayLevel::WHITE, SCALE);
        _drawLineText(textX_bj, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        break;
    }

    case MD_TASK_SCHEDULED: {
        _display.drawCharGray(margin + 2, glyphY, '<', GrayLevel::BLACK, GrayLevel::WHITE, SCALE);
        _drawLineText(textX_bj, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        break;
    }

    // ── XJL signifier types ───────────────────────────────────────────────────

    case MD_PRIORITY: {
        _display.drawCharGray(margin + 2, glyphY, '!', GrayLevel::BLACK, GrayLevel::WHITE, SCALE);
        _drawLineText(textX_bj, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        break;
    }

    case MD_EVENT: {
        _display.drawCharGray(margin + 2, glyphY, '@', GrayLevel::BLACK, GrayLevel::WHITE, SCALE);
        _drawLineText(textX_bj, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        break;
    }

    case MD_QUESTION: {
        _display.drawCharGray(margin + 2, glyphY, '?', GrayLevel::BLACK, GrayLevel::WHITE, SCALE);
        _drawLineText(textX_bj, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        break;
    }

    // ── XJL Extended Markdown types ───────────────────────────────────────────

    case MD_CODE_BLOCK: {
        // DARK_GRAY 3 px left bar + 1-char indent
        _display.fillRectGray(margin, y, 3, _itemH, GrayLevel::DARK_GRAY);
        _display.drawTextGray(margin + charAdv + 4, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE);
        break;
    }

    case MD_BULLET_NESTED: {
        if (!line.continuation) {
            _display.drawCharGray(margin + charAdv * 2, y, '-', GrayLevel::BLACK, GrayLevel::WHITE, SCALE);
        }
        _drawLineText(margin + charAdv * 4, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        break;
    }

    // ── GFM table types ───────────────────────────────────────────────────────

    case MD_TABLE_HEADER: {
        if (line.cells.empty()) {
            _display.drawTextGray(margin, y, line.text.c_str(), GrayLevel::WHITE, GrayLevel::DARK_GRAY, SCALE);
            break;
        }
        uint16_t numCols = (uint16_t)line.cells.size();
        uint16_t colW    = (uint16_t)((dispW - margin * 2) / numCols);
        // DARK_GRAY background for the header row
        _display.fillRectGray(0, y, dispW, _itemH, GrayLevel::DARK_GRAY);
        for (uint16_t c = 0; c < numCols; c++) {
            uint16_t cx       = margin + c * colW;
            uint16_t maxCh    = (colW > charAdv * 2u) ? (colW - charAdv) / charAdv : 1u;
            String   cell     = line.cells[c];
            if ((uint16_t)cell.length() > maxCh) cell = cell.substring(0, maxCh);
            _display.drawTextGray(cx + 2, y, cell.c_str(), GrayLevel::WHITE, GrayLevel::DARK_GRAY, SCALE);
            // LIGHT_GRAY vertical divider between columns
            if (c + 1 < numCols) {
                _display.fillRectGray(cx + colW - 1, y, 1, _itemH, GrayLevel::LIGHT_GRAY);
            }
        }
        break;
    }

    case MD_TABLE_SEP: {
        // DARK_GRAY full-width 1 px horizontal separator
        uint16_t ruleY = y + _itemH / 2;
        _display.fillRectGray(margin, ruleY, dispW - margin * 2, 1, GrayLevel::DARK_GRAY);
        break;
    }

    case MD_TABLE_ROW: {
        if (line.cells.empty()) {
            _drawLineText(margin, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
            break;
        }
        uint16_t numCols = (uint16_t)line.cells.size();
        uint16_t colW    = (uint16_t)((dispW - margin * 2) / numCols);
        // DARK_GRAY bottom border for each data row
        _display.fillRectGray(margin, y + _itemH - 1, dispW - margin * 2, 1, GrayLevel::DARK_GRAY);
        for (uint16_t c = 0; c < numCols; c++) {
            uint16_t cx    = margin + c * colW;
            uint16_t maxCh = (colW > charAdv * 2u) ? (colW - charAdv) / charAdv : 1u;
            String   cell  = line.cells[c];
            if ((uint16_t)cell.length() > maxCh) cell = cell.substring(0, maxCh);
            _display.drawTextGray(cx + 2, y, cell.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE);
            // DARK_GRAY vertical divider between columns
            if (c + 1 < numCols) {
                _display.fillRectGray(cx + colW - 1, y, 1, _itemH, GrayLevel::DARK_GRAY);
            }
        }
        break;
    }

    // ── XJL callout blocks ────────────────────────────────────────────────────

    case MD_CALLOUT_NOTE:
    case MD_CALLOUT_TIP:
    case MD_CALLOUT_WARNING:
    case MD_CALLOUT_IMPORTANT: {
        const char* label = "NOTE";
        if      (line.type == MD_CALLOUT_TIP)       label = "TIP";
        else if (line.type == MD_CALLOUT_WARNING)   label = "WARN";
        else if (line.type == MD_CALLOUT_IMPORTANT) label = "IMP!";

        // DARK_GRAY 3 px left bar (same as code block)
        _display.fillRectGray(margin, y, 3, _itemH, GrayLevel::DARK_GRAY);

        uint16_t labelW    = (uint16_t)(strlen(label) * charAdv + 4);
        uint16_t textStart = margin + 4 + labelW + 2;

        if (!line.continuation) {
            // DARK_GRAY inverted badge: [ LABEL ]
            _display.fillRectGray(margin + 4, y + 1, labelW, _itemH - 2, GrayLevel::DARK_GRAY);
            _display.drawTextGray(margin + 6, y + 1, label, GrayLevel::WHITE, GrayLevel::DARK_GRAY, SCALE);
        }
        _drawLineText(textStart, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        break;
    }

    // ── XJL definition list ───────────────────────────────────────────────────

    case MD_DEFLIST_TERM: {
        _drawLineText(margin, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        break;
    }

    case MD_DEFLIST_DEF: {
        // DARK_GRAY 2 px left bar + 1-char indent
        _display.fillRectGray(margin, y, 2, _itemH, GrayLevel::DARK_GRAY);
        _drawLineText(margin + charAdv + 4, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        break;
    }

    // ── XJL habit / data grid ─────────────────────────────────────────────────

    case MD_GRID_HEADER: {
        if (line.cells.empty()) break;
        uint16_t numCols = (uint16_t)line.cells.size();
        uint16_t colW    = (uint16_t)((dispW - margin * 2) / numCols);
        // DARK_GRAY header bar
        _display.fillRectGray(0, y, dispW, _itemH, GrayLevel::DARK_GRAY);
        for (uint16_t c = 0; c < numCols; c++) {
            uint16_t cx    = margin + c * colW;
            uint16_t maxCh = (colW > charAdv * 2u) ? (colW - charAdv) / charAdv : 1u;
            String   cell  = line.cells[c];
            if ((uint16_t)cell.length() > maxCh) cell = cell.substring(0, maxCh);
            _display.drawTextGray(cx + 2, y, cell.c_str(), GrayLevel::WHITE, GrayLevel::DARK_GRAY, SCALE);
            if (c + 1 < numCols) {
                _display.fillRectGray(cx + colW - 1, y, 1, _itemH, GrayLevel::LIGHT_GRAY); // LIGHT_GRAY divider
            }
        }
        break;
    }

    case MD_GRID_ROW: {
        if (line.cells.empty()) break;
        uint16_t numCols = (uint16_t)line.cells.size();
        uint16_t colW    = (uint16_t)((dispW - margin * 2) / numCols);
        // DARK_GRAY bottom border
        _display.fillRectGray(margin, y + _itemH - 1, dispW - margin * 2, 1, GrayLevel::DARK_GRAY);

        for (uint16_t c = 0; c < numCols; c++) {
            uint16_t cx = margin + c * colW;
            String trimCell = line.cells[c];
            trimCell.trim();

            if (c == 0) {
                // Label column: left-aligned text
                uint16_t maxCh = (colW > charAdv * 2u) ? (colW - charAdv) / charAdv : 1u;
                if ((uint16_t)trimCell.length() > maxCh) trimCell = trimCell.substring(0, maxCh);
                _drawLineText(cx + 2, y, trimCell.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
            } else {
                // Data cell: draw filled or empty box centred in the column
                uint16_t cellCx  = cx + (colW > BOX_SZ ? (colW - BOX_SZ) / 2 : 0);
                uint16_t cellCy  = y  + BOX_OFF;
                bool     filled  = (trimCell == "x" || trimCell == "X" || trimCell == "1");
                bool     isEmpty = (trimCell == "." || trimCell == "0" ||
                                    trimCell.length() == 0 || trimCell == " ");

                if (filled || isEmpty) {
                    // BLACK border
                    _display.fillRectGray(cellCx,              cellCy,              BOX_SZ, 1,      GrayLevel::BLACK);
                    _display.fillRectGray(cellCx,              cellCy + BOX_SZ - 1, BOX_SZ, 1,      GrayLevel::BLACK);
                    _display.fillRectGray(cellCx,              cellCy,              1,      BOX_SZ, GrayLevel::BLACK);
                    _display.fillRectGray(cellCx + BOX_SZ - 1, cellCy,              1,      BOX_SZ, GrayLevel::BLACK);
                    if (filled) {
                        // DARK_GRAY fill (softer than solid black)
                        _display.fillRectGray(cellCx + 2, cellCy + 2, BOX_SZ - 4, BOX_SZ - 4, GrayLevel::DARK_GRAY);
                    }
                } else {
                    // Short text value — centre in cell
                    uint16_t maxCh = (colW > charAdv * 2u) ? (colW - charAdv) / charAdv : 1u;
                    if ((uint16_t)trimCell.length() > maxCh) trimCell = trimCell.substring(0, maxCh);
                    uint16_t tw = (uint16_t)(trimCell.length() * charAdv);
                    uint16_t tx = cx + (colW > tw ? (colW - tw) / 2 : 2);
                    _display.drawTextGray(tx, y, trimCell.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE);
                }
            }

            // DARK_GRAY vertical divider between columns
            if (c + 1 < numCols) {
                _display.fillRectGray(cx + colW - 1, y, 1, _itemH, GrayLevel::DARK_GRAY);
            }
        }
        break;
    }

    case MD_BLANK:
    case MD_NORMAL:
    default:
        _drawLineText(margin, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        break;

    // ── XJL Theme System ──────────────────────────────────────────────────────

    case MD_THEME: {
        // DARK_GRAY full-width bar with a "THEME" badge on the left
        _display.fillRectGray(0, y, dispW, _itemH, GrayLevel::DARK_GRAY);
        const char* badge = "THEME";
        uint16_t    bW    = (uint16_t)(strlen(badge) * charAdv + 4);
        _display.drawTextGray(margin + 2, y, badge, GrayLevel::WHITE, GrayLevel::DARK_GRAY, SCALE);
        uint16_t textStart = margin + bW + 4;
        if (!line.text.isEmpty()) {
            _drawLineText(textStart, y, line.text.c_str(), GrayLevel::WHITE, GrayLevel::DARK_GRAY, SCALE, line);
        }
        break;
    }

    case MD_SEASON: {
        // DARK_GRAY full-width bar with a "SEASON" badge on the left
        _display.fillRectGray(0, y, dispW, _itemH, GrayLevel::DARK_GRAY);
        const char* badge = "SEASON";
        uint16_t    bW    = (uint16_t)(strlen(badge) * charAdv + 4);
        _display.drawTextGray(margin + 2, y, badge, GrayLevel::WHITE, GrayLevel::DARK_GRAY, SCALE);
        uint16_t textStart = margin + bW + 4;
        if (!line.text.isEmpty()) {
            _drawLineText(textStart, y, line.text.c_str(), GrayLevel::WHITE, GrayLevel::DARK_GRAY, SCALE, line);
        }
        break;
    }

    case MD_RATING: {
        // "N/5" label followed by five filled/empty boxes
        int n = atoi(line.text.c_str());
        if (n < 1) n = 1;
        if (n > 5) n = 5;

        // Label: "N/5"
        char label[5];
        snprintf(label, sizeof(label), "%d/5", n);
        uint16_t labelW  = (uint16_t)(strlen(label) * charAdv);
        _display.drawTextGray(margin, y, label, GrayLevel::BLACK, GrayLevel::WHITE, SCALE);

        // Five boxes drawn to the right of the label
        uint16_t bx = margin + labelW + charAdv; // one-char gap after label
        for (int k = 1; k <= 5; k++) {
            uint16_t cellCy = y + BOX_OFF;
            bool filled = (k <= n);
            // BLACK border
            _display.fillRectGray(bx,              cellCy,              BOX_SZ, 1,      GrayLevel::BLACK);
            _display.fillRectGray(bx,              cellCy + BOX_SZ - 1, BOX_SZ, 1,      GrayLevel::BLACK);
            _display.fillRectGray(bx,              cellCy,              1,      BOX_SZ, GrayLevel::BLACK);
            _display.fillRectGray(bx + BOX_SZ - 1, cellCy,              1,      BOX_SZ, GrayLevel::BLACK);
            if (filled) {
                // DARK_GRAY fill
                _display.fillRectGray(bx + 2, cellCy + 2, BOX_SZ - 4, BOX_SZ - 4, GrayLevel::DARK_GRAY);
            }
            bx += (uint16_t)(BOX_SZ + 2); // 2 px gap between boxes
        }
        break;
    }

    case MD_THEME_NOTE: {
        // Tilde glyph in the marker zone, text to the right
        _display.drawCharGray(margin + 2, glyphY, '~', GrayLevel::BLACK, GrayLevel::WHITE, SCALE);
        _drawLineText(textX_bj, y, line.text.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE, line);
        break;
    }
    }
}

void EntryScreen::_renderPage(const String& title, const std::vector<MdLine>& lines,
                               int topLine, int totalLines) {
    uint16_t dispW = _display.width();
    uint16_t dispH = _display.height();

    // Clear to white (both BW and RED planes)
    _display.clearFrameGrayscale();

    // ── Title ─────────────────────────────────────────────────────────────────
    _display.drawTextGray(4, 4, title.c_str(), GrayLevel::BLACK, GrayLevel::WHITE, SCALE);

    // DARK_GRAY separator
    _display.fillRectGray(0, _titleH, dispW, 1, GrayLevel::DARK_GRAY);

    // ── Body lines ────────────────────────────────────────────────────────────
    const int linesPerPage = (dispH - _bodyY - 4) / _itemH;
    for (int i = 0; i < linesPerPage; i++) {
        int lineIdx = topLine + i;
        if (lineIdx >= (int)lines.size()) break;
        uint16_t lineY = (uint16_t)(_bodyY + i * _itemH);
        _renderLine(dispW, lines[lineIdx], lineY);
    }

    // ── Page indicator (bottom-right) ─────────────────────────────────────────
    if (totalLines > linesPerPage) {
        char pageInfo[16];
        int currentPage = topLine / linesPerPage + 1;
        int totalPages  = (totalLines + linesPerPage - 1) / linesPerPage;
        snprintf(pageInfo, sizeof(pageInfo), "%d/%d", currentPage, totalPages);
        uint16_t piW = (uint16_t)(strlen(pageInfo) * _display.charAdvance(SCALE));
        _display.drawTextGray(dispW - piW - 4, dispH - _itemH - 2,
                              pageInfo, GrayLevel::BLACK, GrayLevel::WHITE, SCALE);
    }
}
