// ─────────────────────────────────────────────────────────────────────────────
// BrowseScreen.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "BrowseScreen.h"
#include <esp_sleep.h>

BrowseScreen::BrowseScreen(JournalManager& jm, X4Display& display,
                            X4Input& input, X4Clock& clock)
    : _jm(jm), _display(display), _input(input), _clock(clock)
{}

BrowseResult BrowseScreen::run(String& outPath) {
    outPath = "";

    // Fetch entries for the current month
    uint16_t year; uint8_t month;
    _clock.currentYearMonth(year, month);
    std::vector<String> paths = _jm.listEntries(year, month);

    // Build display labels: item 0 is always "[ + NEW ENTRY ]"
    std::vector<String> labels;
    labels.reserve(paths.size() + 1);
    labels.push_back("[ + NEW ENTRY ]");
    for (auto& p : paths) {
        labels.push_back(_jm.getEntryTitle(p));
    }

    const int totalItems = (int)labels.size();
    const int dispH      = _display.height();
    const int linesPerPage = (dispH - HEADER_H) / ITEM_H;

    int selected = 0;
    int topRow   = 0;
    bool needRedraw = true;
    bool fullRefreshPending = true;

    while (true) {
        _input.tick();

        // Power-button deep sleep
        if (_input.isPowerButtonPressed()) {
            _display.sleep();
            esp_deep_sleep_start();
        }

        if (_input.wasUp()) {
            if (selected > 0) { selected--; needRedraw = true; }
            if (selected < topRow) topRow = selected;
        }
        if (_input.wasDown()) {
            if (selected < totalItems - 1) { selected++; needRedraw = true; }
            if (selected >= topRow + linesPerPage) topRow = selected - linesPerPage + 1;
        }
        if (_input.wasConfirm()) {
            if (selected == 0) {
                return BrowseResult::NEW_ENTRY;
            } else {
                outPath = paths[selected - 1]; // offset by 1 for the NEW item
                return BrowseResult::OPEN_ENTRY;
            }
        }
        if (_input.wasBack()) {
            return BrowseResult::BACK;
        }

        if (needRedraw) {
            _render(labels, selected, topRow);
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

void BrowseScreen::_render(const std::vector<String>& labels,
                            int selected, int topRow) {
    uint8_t* fb    = _display.getFrameBuffer();
    uint16_t dispW = _display.width();
    uint16_t dispH = _display.height();

    // Clear to white
    memset(fb, 0xFF, (dispW / 8) * dispH);

    // ── Header ───────────────────────────────────────────────────────────────
    _display.drawText(fb, 4, 4, "JOURNAL", false, SCALE);

    // Current month label on the right side
    uint16_t year; uint8_t month;
    _clock.currentYearMonth(year, month);
    char monthBuf[12];
    snprintf(monthBuf, sizeof(monthBuf), "%04u-%02u", year, month);
    uint16_t monthLabelW = (uint16_t)(strlen(monthBuf) * FONT5X7_ADVANCE * SCALE);
    _display.drawText(fb, dispW - monthLabelW - 4, 4, monthBuf, false, SCALE);

    // Separator line
    const uint16_t sepY = HEADER_H - 2;
    _display.fillRect(fb, 0, sepY, dispW, 1, true);

    // ── Entry list ────────────────────────────────────────────────────────────
    const int visibleCount = (int)labels.size();
    for (int i = 0; i < (int)((dispH - HEADER_H) / ITEM_H); i++) {
        int idx = topRow + i;
        if (idx >= visibleCount) break;

        uint16_t itemY = HEADER_H + (uint16_t)(i * ITEM_H);
        bool sel = (idx == selected);

        if (sel) {
            // Highlight bar
            _display.fillRect(fb, 0, itemY, dispW, ITEM_H, true);
        }
        _display.drawText(fb, 4, itemY + 2, labels[idx].c_str(), sel, SCALE);
    }
}
