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
            if (fullRefreshPending) {
                _display.fullRefresh();
                fullRefreshPending = false;
            } else {
                _display.fastRefresh();
            }
            needRedraw = false;
        }

        delay(10);
    }
}

// ── _drawLineText ─────────────────────────────────────────────────────────────

void EntryScreen::_drawLineText(uint8_t* fb, uint16_t x, uint16_t y,
                                 const char* str, bool inverted, uint8_t scale,
                                 const MdLine& line) {
    // Inline-code border box: 1 px frame around the text glyph area.
    // Only drawn when not on an already-inverted background.
    if (line.inlineCode && !inverted) {
        uint16_t tw = (uint16_t)(strlen(str) * _display.charAdvance(scale));
        uint16_t gh = _display.glyphHeight(scale);
        if (tw > 0) {
            _display.fillRect(fb, x - 1, y,         tw + 2, 1,      true); // top
            _display.fillRect(fb, x - 1, y + gh,     tw + 2, 1,      true); // bottom
            _display.fillRect(fb, x - 1, y,         1,      gh + 1, true); // left
            _display.fillRect(fb, x + tw, y,         1,      gh + 1, true); // right
        }
    }
    _display.drawText(fb, x, y, str, inverted, scale);
    // Faux-bold: draw again 1 px to the right.
    if (line.bold) {
        _display.drawText(fb, x + 1, y, str, inverted, scale);
    }
}

// ── _renderLine ───────────────────────────────────────────────────────────────

void EntryScreen::_renderLine(uint8_t* fb, uint16_t dispW,
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
        // Inverted: fill the full-width bar black, draw text in white
        _display.fillRect(fb, 0, y, dispW, _itemH, true);
        _drawLineText(fb, margin, y, line.text.c_str(), /*inverted=*/true, SCALE, line);
        break;
    }

    case MD_H2: {
        _drawLineText(fb, margin, y, line.text.c_str(), false, SCALE, line);
        // Underline: 1 px below the glyph body
        uint16_t underY = y + _display.glyphHeight(SCALE) + 1;
        _display.fillRect(fb, margin, underY, dispW - margin * 2, 1, true);
        break;
    }

    case MD_H3: {
        // 1-char indent
        _drawLineText(fb, margin + charAdv, y, line.text.c_str(), false, SCALE, line);
        break;
    }

    case MD_BULLET: {
        if (!line.continuation) {
            // Draw the bullet dash then the text further right
            _display.drawChar(fb, margin, y, '-', false, SCALE);
        }
        _drawLineText(fb, margin + charAdv * 2, y, line.text.c_str(), false, SCALE, line);
        break;
    }

    case MD_ORDERED: {
        if (!line.continuation) {
            // First line includes the "N. " prefix — render from left margin
            _drawLineText(fb, margin, y, line.text.c_str(), false, SCALE, line);
        } else {
            // Continuation: indent by 3 chars (aligns under text after "N. ")
            _drawLineText(fb, margin + charAdv * 3, y, line.text.c_str(), false, SCALE, line);
        }
        break;
    }

    case MD_BLOCKQUOTE: {
        // 2 px left bar
        _display.fillRect(fb, margin, y, 2, _itemH, true);
        _drawLineText(fb, margin + charAdv + 4, y, line.text.c_str(), false, SCALE, line);
        break;
    }

    case MD_HLINE: {
        // Centred 2 px horizontal rule
        uint16_t ruleY = y + _itemH / 2 - 1;
        _display.fillRect(fb, margin, ruleY, dispW - margin * 2, 2, true);
        break;
    }

    // ── XJL task types ────────────────────────────────────────────────────────

    case MD_TASK_OPEN: {
        // Draw empty checkbox outline (10×10 px)
        _display.fillRect(fb, boxX,              boxY,              BOX_SZ, 1,      true); // top
        _display.fillRect(fb, boxX,              boxY + BOX_SZ - 1, BOX_SZ, 1,      true); // bottom
        _display.fillRect(fb, boxX,              boxY,              1,      BOX_SZ, true); // left
        _display.fillRect(fb, boxX + BOX_SZ - 1, boxY,              1,      BOX_SZ, true); // right
        _drawLineText(fb, textX_bj, y, line.text.c_str(), false, SCALE, line);
        break;
    }

    case MD_TASK_DONE: {
        // Draw filled checkbox (outer border + inner fill)
        _display.fillRect(fb, boxX,              boxY,              BOX_SZ, 1,      true); // top
        _display.fillRect(fb, boxX,              boxY + BOX_SZ - 1, BOX_SZ, 1,      true); // bottom
        _display.fillRect(fb, boxX,              boxY,              1,      BOX_SZ, true); // left
        _display.fillRect(fb, boxX + BOX_SZ - 1, boxY,              1,      BOX_SZ, true); // right
        _display.fillRect(fb, boxX + 2,          boxY + 2,          BOX_SZ - 4, BOX_SZ - 4, true); // fill
        _drawLineText(fb, textX_bj, y, line.text.c_str(), false, SCALE, line);
        // Strikethrough: 1 px line at glyph vertical midpoint
        if (line.strikethrough) {
            uint16_t tLen    = (uint16_t)line.text.length();
            uint16_t tW      = (uint16_t)(tLen * charAdv);
            uint16_t maxW    = (dispW > textX_bj + 4u) ? dispW - textX_bj - 4u : 0u;
            if (tW > maxW) tW = maxW;
            uint16_t strikeY = y + _display.glyphHeight(SCALE) / 2;
            if (tW > 0) _display.fillRect(fb, textX_bj, strikeY, tW, 1, true);
        }
        break;
    }

    case MD_TASK_MIGRATED: {
        // Forward arrow: draw '>' glyph, text to right
        _display.drawChar(fb, margin + 2, glyphY, '>', false, SCALE);
        _drawLineText(fb, textX_bj, y, line.text.c_str(), false, SCALE, line);
        break;
    }

    case MD_TASK_SCHEDULED: {
        // Back arrow: draw '<' glyph, text to right
        _display.drawChar(fb, margin + 2, glyphY, '<', false, SCALE);
        _drawLineText(fb, textX_bj, y, line.text.c_str(), false, SCALE, line);
        break;
    }

    // ── XJL signifier types ───────────────────────────────────────────────────

    case MD_PRIORITY: {
        _display.drawChar(fb, margin + 2, glyphY, '!', false, SCALE);
        _drawLineText(fb, textX_bj, y, line.text.c_str(), false, SCALE, line);
        break;
    }

    case MD_EVENT: {
        _display.drawChar(fb, margin + 2, glyphY, '@', false, SCALE);
        _drawLineText(fb, textX_bj, y, line.text.c_str(), false, SCALE, line);
        break;
    }

    case MD_QUESTION: {
        _display.drawChar(fb, margin + 2, glyphY, '?', false, SCALE);
        _drawLineText(fb, textX_bj, y, line.text.c_str(), false, SCALE, line);
        break;
    }

    // ── XJL Extended Markdown types ───────────────────────────────────────────

    case MD_CODE_BLOCK: {
        // 3 px left bar (thicker than blockquote's 2 px) + 1-char indent
        _display.fillRect(fb, margin, y, 3, _itemH, true);
        _display.drawText(fb, margin + charAdv + 4, y, line.text.c_str(), false, SCALE);
        break;
    }

    case MD_BULLET_NESTED: {
        if (!line.continuation) {
            // Draw a dash marker at 2-char indent (inset from the outer bullet)
            _display.drawChar(fb, margin + charAdv * 2, y, '-', false, SCALE);
        }
        // Text starts at 4-char indent
        _drawLineText(fb, margin + charAdv * 4, y, line.text.c_str(), false, SCALE, line);
        break;
    }

    // ── GFM table types ───────────────────────────────────────────────────────

    case MD_TABLE_HEADER: {
        if (line.cells.empty()) {
            _display.drawText(fb, margin, y, line.text.c_str(), true, SCALE);
            break;
        }
        uint16_t numCols = (uint16_t)line.cells.size();
        uint16_t colW    = (uint16_t)((dispW - margin * 2) / numCols);
        // Inverted background for the header row
        _display.fillRect(fb, 0, y, dispW, _itemH, true);
        for (uint16_t c = 0; c < numCols; c++) {
            uint16_t cx       = margin + c * colW;
            uint16_t maxCh    = (colW > charAdv * 2u) ? (colW - charAdv) / charAdv : 1u;
            String   cell     = line.cells[c];
            if ((uint16_t)cell.length() > maxCh) cell = cell.substring(0, maxCh);
            _display.drawText(fb, cx + 2, y, cell.c_str(), /*inverted=*/true, SCALE);
            // White vertical divider between columns
            if (c + 1 < numCols) {
                _display.fillRect(fb, cx + colW - 1, y, 1, _itemH, false);
            }
        }
        break;
    }

    case MD_TABLE_SEP: {
        // Full-width 1 px horizontal separator
        uint16_t ruleY = y + _itemH / 2;
        _display.fillRect(fb, margin, ruleY, dispW - margin * 2, 1, true);
        break;
    }

    case MD_TABLE_ROW: {
        if (line.cells.empty()) {
            _drawLineText(fb, margin, y, line.text.c_str(), false, SCALE, line);
            break;
        }
        uint16_t numCols = (uint16_t)line.cells.size();
        uint16_t colW    = (uint16_t)((dispW - margin * 2) / numCols);
        // Bottom border for each data row
        _display.fillRect(fb, margin, y + _itemH - 1, dispW - margin * 2, 1, true);
        for (uint16_t c = 0; c < numCols; c++) {
            uint16_t cx    = margin + c * colW;
            uint16_t maxCh = (colW > charAdv * 2u) ? (colW - charAdv) / charAdv : 1u;
            String   cell  = line.cells[c];
            if ((uint16_t)cell.length() > maxCh) cell = cell.substring(0, maxCh);
            _display.drawText(fb, cx + 2, y, cell.c_str(), false, SCALE);
            // Vertical divider between columns
            if (c + 1 < numCols) {
                _display.fillRect(fb, cx + colW - 1, y, 1, _itemH, true);
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

        // 3 px left bar (same as code block)
        _display.fillRect(fb, margin, y, 3, _itemH, true);

        uint16_t labelW    = (uint16_t)(strlen(label) * charAdv + 4);
        uint16_t textStart = margin + 4 + labelW + 2;

        if (!line.continuation) {
            // Inverted badge: [ LABEL ]
            _display.fillRect(fb, margin + 4, y + 1, labelW, _itemH - 2, true);
            _display.drawText(fb, margin + 6, y + 1, label, /*inverted=*/true, SCALE);
        }
        _drawLineText(fb, textStart, y, line.text.c_str(), false, SCALE, line);
        break;
    }

    // ── XJL definition list ───────────────────────────────────────────────────

    case MD_DEFLIST_TERM: {
        // Term always renders faux-bold (line.bold is set during post-processing)
        _drawLineText(fb, margin, y, line.text.c_str(), false, SCALE, line);
        break;
    }

    case MD_DEFLIST_DEF: {
        // 2 px left bar + 1-char indent (same visual weight as blockquote)
        _display.fillRect(fb, margin, y, 2, _itemH, true);
        _drawLineText(fb, margin + charAdv + 4, y, line.text.c_str(), false, SCALE, line);
        break;
    }

    // ── XJL habit / data grid ─────────────────────────────────────────────────

    case MD_GRID_HEADER: {
        if (line.cells.empty()) break;
        uint16_t numCols = (uint16_t)line.cells.size();
        uint16_t colW    = (uint16_t)((dispW - margin * 2) / numCols);
        // Inverted header bar
        _display.fillRect(fb, 0, y, dispW, _itemH, true);
        for (uint16_t c = 0; c < numCols; c++) {
            uint16_t cx    = margin + c * colW;
            uint16_t maxCh = (colW > charAdv * 2u) ? (colW - charAdv) / charAdv : 1u;
            String   cell  = line.cells[c];
            if ((uint16_t)cell.length() > maxCh) cell = cell.substring(0, maxCh);
            _display.drawText(fb, cx + 2, y, cell.c_str(), /*inverted=*/true, SCALE);
            if (c + 1 < numCols) {
                _display.fillRect(fb, cx + colW - 1, y, 1, _itemH, false); // white divider
            }
        }
        break;
    }

    case MD_GRID_ROW: {
        if (line.cells.empty()) break;
        uint16_t numCols = (uint16_t)line.cells.size();
        uint16_t colW    = (uint16_t)((dispW - margin * 2) / numCols);
        // Bottom border
        _display.fillRect(fb, margin, y + _itemH - 1, dispW - margin * 2, 1, true);

        for (uint16_t c = 0; c < numCols; c++) {
            uint16_t cx = margin + c * colW;
            String trimCell = line.cells[c];
            trimCell.trim();

            if (c == 0) {
                // Label column: left-aligned text
                uint16_t maxCh = (colW > charAdv * 2u) ? (colW - charAdv) / charAdv : 1u;
                if ((uint16_t)trimCell.length() > maxCh) trimCell = trimCell.substring(0, maxCh);
                _drawLineText(fb, cx + 2, y, trimCell.c_str(), false, SCALE, line);
            } else {
                // Data cell: draw filled or empty box centred in the column
                uint16_t cellCx  = cx + (colW > BOX_SZ ? (colW - BOX_SZ) / 2 : 0);
                uint16_t cellCy  = y  + BOX_OFF;
                bool     filled  = (trimCell == "x" || trimCell == "X" || trimCell == "1");
                bool     isEmpty = (trimCell == "." || trimCell == "0" ||
                                    trimCell.length() == 0 || trimCell == " ");

                if (filled || isEmpty) {
                    // Border
                    _display.fillRect(fb, cellCx,              cellCy,              BOX_SZ, 1,      true);
                    _display.fillRect(fb, cellCx,              cellCy + BOX_SZ - 1, BOX_SZ, 1,      true);
                    _display.fillRect(fb, cellCx,              cellCy,              1,      BOX_SZ, true);
                    _display.fillRect(fb, cellCx + BOX_SZ - 1, cellCy,              1,      BOX_SZ, true);
                    if (filled) {
                        _display.fillRect(fb, cellCx + 2, cellCy + 2,
                                          BOX_SZ - 4, BOX_SZ - 4, true);
                    }
                } else {
                    // Short text value — centre in cell
                    uint16_t maxCh = (colW > charAdv * 2u) ? (colW - charAdv) / charAdv : 1u;
                    if ((uint16_t)trimCell.length() > maxCh) trimCell = trimCell.substring(0, maxCh);
                    uint16_t tw = (uint16_t)(trimCell.length() * charAdv);
                    uint16_t tx = cx + (colW > tw ? (colW - tw) / 2 : 2);
                    _display.drawText(fb, tx, y, trimCell.c_str(), false, SCALE);
                }
            }

            // Vertical divider between columns
            if (c + 1 < numCols) {
                _display.fillRect(fb, cx + colW - 1, y, 1, _itemH, true);
            }
        }
        break;
    }

    case MD_BLANK:
    case MD_NORMAL:
    default:
        _drawLineText(fb, margin, y, line.text.c_str(), false, SCALE, line);
        break;
    }
}

void EntryScreen::_renderPage(const String& title, const std::vector<MdLine>& lines,
                               int topLine, int totalLines) {
    uint8_t* fb    = _display.getFrameBuffer();
    uint16_t dispW = _display.width();
    uint16_t dispH = _display.height();

    // Clear to white
    memset(fb, 0xFF, (dispW / 8) * dispH);

    // ── Title ─────────────────────────────────────────────────────────────────
    _display.drawText(fb, 4, 4, title.c_str(), false, SCALE);

    // Separator
    _display.fillRect(fb, 0, _titleH, dispW, 1, true);

    // ── Body lines ────────────────────────────────────────────────────────────
    const int linesPerPage = (dispH - _bodyY - 4) / _itemH;
    for (int i = 0; i < linesPerPage; i++) {
        int lineIdx = topLine + i;
        if (lineIdx >= (int)lines.size()) break;
        uint16_t lineY = (uint16_t)(_bodyY + i * _itemH);
        _renderLine(fb, dispW, lines[lineIdx], lineY);
    }

    // ── Page indicator (bottom-right) ─────────────────────────────────────────
    if (totalLines > linesPerPage) {
        char pageInfo[16];
        int currentPage = topLine / linesPerPage + 1;
        int totalPages  = (totalLines + linesPerPage - 1) / linesPerPage;
        snprintf(pageInfo, sizeof(pageInfo), "%d/%d", currentPage, totalPages);
        uint16_t piW = (uint16_t)(strlen(pageInfo) * _display.charAdvance(SCALE));
        _display.drawText(fb, dispW - piW - 4, dispH - _itemH - 2,
                          pageInfo, false, SCALE);
    }
}
