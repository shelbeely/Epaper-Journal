// ─────────────────────────────────────────────────────────────────────────────
// EntryScreen.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "EntryScreen.h"
#include <esp_sleep.h>

EntryScreen::EntryScreen(X4Display& display, X4Input& input)
    : _display(display), _input(input)
{}

void EntryScreen::show(const JournalEntry& entry) {
    uint16_t dispW = _display.width();
    uint16_t dispH = _display.height();

    // Characters per line for body text at SCALE
    uint16_t maxChars = (dispW - 8) / (FONT5X7_ADVANCE * SCALE);

    std::vector<String> lines = wrapText(entry.body, maxChars);
    const int totalLines  = (int)lines.size();
    const int linesPerPage = (dispH - BODY_Y - 4) / ITEM_H;
    int topLine = 0;
    bool needRedraw = true;
    bool fullRefreshPending = true;

    while (true) {
        _input.tick();

        // Power-button deep sleep
        if (_input.isPowerButtonPressed()) {
            _display.sleep();
            esp_deep_sleep_start();
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

/*static*/
std::vector<String> EntryScreen::wrapText(const String& text, uint16_t maxCharsPerLine) {
    std::vector<String> result;
    if (maxCharsPerLine == 0) maxCharsPerLine = 1;

    int len = (int)text.length();
    int i   = 0;
    while (i < len) {
        // Handle explicit newline
        if (text[i] == '\n') {
            result.push_back("");
            i++;
            continue;
        }
        // Build one line up to maxCharsPerLine characters, breaking at word boundaries
        int lineEnd = i;
        int lastSpace = -1;
        while (lineEnd < len && (lineEnd - i) < (int)maxCharsPerLine) {
            if (text[lineEnd] == '\n') break;
            if (text[lineEnd] == ' ') lastSpace = lineEnd;
            lineEnd++;
        }
        if (lineEnd >= len || text[lineEnd] == '\n') {
            // Reached end of text or newline — take everything up to here
            result.push_back(text.substring(i, lineEnd));
            if (lineEnd < len && text[lineEnd] == '\n') lineEnd++; // skip the \n
            i = lineEnd;
        } else if (lastSpace > i) {
            // Break at the last space within the line
            result.push_back(text.substring(i, lastSpace));
            i = lastSpace + 1; // skip the space
        } else {
            // No space found — hard-break at max width
            result.push_back(text.substring(i, lineEnd));
            i = lineEnd;
        }
    }
    return result;
}

void EntryScreen::_renderPage(const String& title, const std::vector<String>& lines,
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
        _display.drawText(fb, 4, lineY, lines[lineIdx].c_str(), false, SCALE);
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
