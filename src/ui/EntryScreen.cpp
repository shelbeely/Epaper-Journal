// ─────────────────────────────────────────────────────────────────────────────
// EntryScreen.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "EntryScreen.h"

EntryScreen::EntryScreen(X4Display& display, X4Input& input,
                         SleepScreen& sleepScreen)
    : _display(display), _input(input), _sleepScreen(sleepScreen)
{}

void EntryScreen::show(const JournalEntry& entry) {
    uint16_t dispW = _display.width();

    // Characters per line for body text at SCALE
    uint16_t maxChars = (dispW - 8) / (FONT5X7_ADVANCE * SCALE);

    std::vector<MdLine> lines = MarkdownParser::parse(entry.body, maxChars);
    const int totalLines  = (int)lines.size();
    const int linesPerPage = (_display.height() - BODY_Y - 4) / ITEM_H;
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

void EntryScreen::_renderLine(uint8_t* fb, uint16_t dispW,
                               const MdLine& line, uint16_t y) {
    const uint16_t charAdv = FONT5X7_ADVANCE * SCALE;
    const uint16_t margin  = 4;

    switch (line.type) {

    case MD_H1: {
        // Inverted: fill the full-width bar black, draw text in white
        _display.fillRect(fb, 0, y, dispW, ITEM_H, true);
        _display.drawText(fb, margin, y, line.text.c_str(), /*inverted=*/true, SCALE);
        break;
    }

    case MD_H2: {
        _display.drawText(fb, margin, y, line.text.c_str(), false, SCALE);
        // Underline: 1 px below the glyph body (glyph height = FONT5X7_GLYPH_H * SCALE)
        uint16_t underY = y + (uint16_t)(FONT5X7_GLYPH_H * SCALE) + 1;
        _display.fillRect(fb, margin, underY, dispW - margin * 2, 1, true);
        break;
    }

    case MD_H3: {
        // 1-char indent
        _display.drawText(fb, margin + charAdv, y, line.text.c_str(), false, SCALE);
        break;
    }

    case MD_BULLET: {
        if (!line.continuation) {
            // Draw the bullet dash then the text further right
            _display.drawChar(fb, margin, y, '-', false, SCALE);
        }
        _display.drawText(fb, margin + charAdv * 2, y, line.text.c_str(), false, SCALE);
        break;
    }

    case MD_ORDERED: {
        if (!line.continuation) {
            // First line includes the "N. " prefix — render from left margin
            _display.drawText(fb, margin, y, line.text.c_str(), false, SCALE);
        } else {
            // Continuation: indent by 3 chars (aligns under text after "N. ")
            _display.drawText(fb, margin + charAdv * 3, y, line.text.c_str(), false, SCALE);
        }
        break;
    }

    case MD_BLOCKQUOTE: {
        // 2 px left bar
        _display.fillRect(fb, margin, y, 2, ITEM_H, true);
        _display.drawText(fb, margin + charAdv + 4, y, line.text.c_str(), false, SCALE);
        break;
    }

    case MD_HLINE: {
        // Centred 2 px horizontal rule
        uint16_t ruleY = y + ITEM_H / 2 - 1;
        _display.fillRect(fb, margin, ruleY, dispW - margin * 2, 2, true);
        break;
    }

    case MD_BLANK:
    case MD_NORMAL:
    default:
        _display.drawText(fb, margin, y, line.text.c_str(), false, SCALE);
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
    _display.fillRect(fb, 0, TITLE_H, dispW, 1, true);

    // ── Body lines ────────────────────────────────────────────────────────────
    const int linesPerPage = (dispH - BODY_Y - 4) / ITEM_H;
    for (int i = 0; i < linesPerPage; i++) {
        int lineIdx = topLine + i;
        if (lineIdx >= (int)lines.size()) break;
        uint16_t lineY = (uint16_t)(BODY_Y + i * ITEM_H);
        _renderLine(fb, dispW, lines[lineIdx], lineY);
    }

    // ── Page indicator (bottom-right) ─────────────────────────────────────────
    if (totalLines > linesPerPage) {
        char pageInfo[16];
        int currentPage = topLine / linesPerPage + 1;
        int totalPages  = (totalLines + linesPerPage - 1) / linesPerPage;
        snprintf(pageInfo, sizeof(pageInfo), "%d/%d", currentPage, totalPages);
        uint16_t piW = (uint16_t)(strlen(pageInfo) * FONT5X7_ADVANCE * SCALE);
        _display.drawText(fb, dispW - piW - 4, dispH - ITEM_H - 2,
                          pageInfo, false, SCALE);
    }
}
