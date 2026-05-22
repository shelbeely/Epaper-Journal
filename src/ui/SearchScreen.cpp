// ─────────────────────────────────────────────────────────────────────────────
// SearchScreen.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "SearchScreen.h"
#include <stdio.h>
#include <string.h>

const char* SearchScreen::CHARSET = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";

SearchScreen::SearchScreen(JournalManager& jm, X4Display& display,
                           X4Input& input, SleepScreen& sleepScreen)
    : _jm(jm), _display(display), _input(input), _sleepScreen(sleepScreen)
{}

String SearchScreen::run() {
    enum class Mode {
        QUERY,
        RESULTS,
    };

    uint8_t slots[QUERY_LEN] = {0};
    uint8_t cursor = 0;
    Mode mode = Mode::QUERY;

    std::vector<String> paths;
    std::vector<String> labels;
    String query;
    int selected = 0;
    int topRow = 0;

    bool needRedraw = true;
    bool fullRefreshPending = true;
    uint32_t lastActivity = millis();

    while (true) {
        _input.tick();

        if (millis() - lastActivity > IDLE_SLEEP_TIMEOUT_MS) {
            _sleepScreen.sleep(0);
        }

        if (_input.isPowerButtonPressed()) {
            _sleepScreen.sleep(0);
        }

        if (mode == Mode::QUERY) {
            bool changed = false;

            if (_input.wasUp()) {
                slots[cursor] = (uint8_t)((slots[cursor] + 1) % _charsetLen());
                changed = true;
            }
            if (_input.wasDown()) {
                slots[cursor] = (uint8_t)((slots[cursor] + _charsetLen() - 1) % _charsetLen());
                changed = true;
            }
            if (_input.wasLeft()) {
                if (cursor > 0) {
                    cursor--;
                    changed = true;
                }
            }
            if (_input.wasRight()) {
                if (cursor + 1 < QUERY_LEN) {
                    cursor++;
                    changed = true;
                }
            }
            if (_input.wasConfirm()) {
                query = _queryFromSlots(slots);
                lastActivity = millis();
                if (!query.isEmpty()) {
                    paths = _jm.searchEntries(query);
                    labels.clear();
                    labels.reserve(paths.size());
                    for (const auto& path : paths) {
                        labels.push_back(_labelForPath(path));
                    }
                    selected = 0;
                    topRow = 0;
                    mode = Mode::RESULTS;
                    needRedraw = true;
                    fullRefreshPending = true;
                }
            }
            if (_input.wasBack()) {
                lastActivity = millis();
                return "";
            }

            if (changed) {
                lastActivity = millis();
                needRedraw = true;
            }
        } else {
            const int totalItems = (int)labels.size();
            const int linesPerPage = (_display.height() - HEADER_H) / ITEM_H;

            if (_input.wasUp()) {
                if (selected > 0) {
                    selected--;
                    needRedraw = true;
                }
                if (selected < topRow) topRow = selected;
                lastActivity = millis();
            }
            if (_input.wasDown()) {
                if (selected < totalItems - 1) {
                    selected++;
                    needRedraw = true;
                }
                if (selected >= topRow + linesPerPage) topRow = selected - linesPerPage + 1;
                lastActivity = millis();
            }
            if (_input.wasConfirm()) {
                lastActivity = millis();
                if (!paths.empty() && selected >= 0 && selected < (int)paths.size()) {
                    return paths[selected];
                }
            }
            if (_input.wasBack()) {
                lastActivity = millis();
                return "";
            }
        }

        if (needRedraw) {
            if (mode == Mode::QUERY) {
                _renderQuery(slots, cursor);
            } else {
                _renderResults(query, labels, selected, topRow);
            }

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
uint8_t SearchScreen::_charsetLen() {
    return (uint8_t)strlen(CHARSET);
}

/*static*/
String SearchScreen::_queryFromSlots(const uint8_t slots[QUERY_LEN]) {
    String query;
    for (uint8_t i = 0; i < QUERY_LEN; i++) {
        query += CHARSET[slots[i]];
    }
    query.trim();
    return query;
}

void SearchScreen::_renderQuery(const uint8_t slots[QUERY_LEN], uint8_t cursor) {
    uint8_t* fb = _display.getFrameBuffer();
    uint16_t dispW = _display.width();
    uint16_t dispH = _display.height();
    String query = _queryFromSlots(slots);

    memset(fb, 0xFF, (dispW / 8) * dispH);

    _display.drawText(fb, 4, 4, "SEARCH", false, SCALE);
    _display.drawText(fb, 4, 26, "UP/DN:char  LR:move  CONFIRM:search  BACK:cancel", false, 1);
    _display.fillRect(fb, 0, HEADER_H - 2, dispW, 1, true);

    const uint16_t boxW = 22;
    const uint16_t boxH = 28;
    const uint16_t gap = 4;
    const uint16_t totalW = QUERY_LEN * boxW + (QUERY_LEN - 1) * gap;
    const uint16_t startX = (dispW - totalW) / 2;
    const uint16_t boxY = HEADER_H + 30;

    for (uint8_t i = 0; i < QUERY_LEN; i++) {
        uint16_t x = startX + i * (boxW + gap);
        bool sel = (i == cursor);
        if (sel) {
            _display.fillRect(fb, x, boxY, boxW, boxH, true);
        } else {
            _display.fillRect(fb, x, boxY, boxW, 1, true);
            _display.fillRect(fb, x, boxY + boxH - 1, boxW, 1, true);
            _display.fillRect(fb, x, boxY, 1, boxH, true);
            _display.fillRect(fb, x + boxW - 1, boxY, 1, boxH, true);
        }

        char ch[2] = {CHARSET[slots[i]], '\0'};
        uint16_t charX = x + (boxW - FONT5X7_ADVANCE * SCALE) / 2;
        uint16_t charY = boxY + 5;
        _display.drawText(fb, charX, charY, ch, sel, SCALE);
    }

    _display.drawText(fb, 4, boxY + boxH + 18,
                      query.isEmpty() ? "Enter a keyword, title, or date." : query.c_str(),
                      false, 1);
}

void SearchScreen::_renderResults(const String& query,
                                  const std::vector<String>& labels,
                                  int selected, int topRow) {
    uint8_t* fb = _display.getFrameBuffer();
    uint16_t dispW = _display.width();
    uint16_t dispH = _display.height();

    memset(fb, 0xFF, (dispW / 8) * dispH);

    _display.drawText(fb, 4, 4, "SEARCH", false, SCALE);
    String prompt = "Q: " + query;
    _display.drawText(fb, 4, 26, prompt.c_str(), false, 1);

    char countLine[16];
    snprintf(countLine, sizeof(countLine), "%u match%s",
             (unsigned)labels.size(), labels.size() == 1 ? "" : "es");
    uint16_t countW = (uint16_t)(strlen(countLine) * FONT5X7_ADVANCE);
    _display.drawText(fb, dispW - countW - 4, 26, countLine, false, 1);
    _display.fillRect(fb, 0, HEADER_H - 2, dispW, 1, true);

    if (labels.empty()) {
        _display.drawText(fb, 4, HEADER_H + 8, "[ NO MATCHES ]", false, SCALE);
        _display.drawText(fb, 4, HEADER_H + 32, "BACK: exit", false, 1);
        return;
    }

    const int visibleRows = (dispH - HEADER_H) / ITEM_H;
    for (int i = 0; i < visibleRows; i++) {
        int idx = topRow + i;
        if (idx >= (int)labels.size()) break;

        uint16_t itemY = HEADER_H + (uint16_t)(i * ITEM_H);
        bool sel = (idx == selected);
        if (sel) {
            _display.fillRect(fb, 0, itemY, dispW, ITEM_H, true);
        }
        _display.drawText(fb, 4, itemY + 2, labels[idx].c_str(), sel, SCALE);
    }
}

String SearchScreen::_labelForPath(const String& path) {
    JournalEntry entry;
    if (_jm.loadEntry(path, entry) && !entry.locked && !entry.title.isEmpty()) {
        return entry.title;
    }
    return JournalManager::labelFromFilename(path);
}
