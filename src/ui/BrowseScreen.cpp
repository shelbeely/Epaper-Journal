// ─────────────────────────────────────────────────────────────────────────────
// BrowseScreen.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "BrowseScreen.h"
#include <WiFi.h>
#include "../journal/PromptPack.h"
#include "../vault/VaultManager.h"

BrowseScreen::BrowseScreen(JournalManager& jm, X4Display& display,
                            X4Input& input, X4Clock& clock,
                            SleepScreen& sleepScreen, VaultManager* vault)
    : _jm(jm), _display(display), _input(input), _clock(clock),
      _sleepScreen(sleepScreen), _vault(vault),
      _itemH(_display.lineHeight(SCALE))
{}

BrowseResult BrowseScreen::run(String& outPath) {
    outPath = "";

    // Fetch entries for the current month
    uint16_t year; uint8_t month;
    _clock.currentYearMonth(year, month);
    std::vector<String> paths = _jm.listEntries(year, month);

    // Build display labels:
    //   item 0 → "[ + NEW ENTRY ]"  (BrowseResult::NEW_ENTRY)
    //   item 1 → "[ STREAK ]"       (BrowseResult::CALENDAR)
    //   item 2 → "[ SEARCH ]"       (BrowseResult::SEARCH)
    //   item 3 → vault toggle       (BrowseResult::VAULT_TOGGLE)
    //   item 4 → wifi toggle        (BrowseResult::WIFI_TOGGLE)
    //   item 4+ → entry titles      (BrowseResult::OPEN_ENTRY)
    std::vector<String> labels;
    labels.reserve(paths.size() + FIXED_ITEMS);
    labels.push_back("[ + NEW ENTRY ]");
    labels.push_back("[ STREAK ]");
    labels.push_back("[ SEARCH ]");
    // Vault toggle item (label depends on lock state)
    if (_vault) {
        labels.push_back(_vault->isUnlocked() ? "[ LOCK VAULT ]" : "[ UNLOCK VAULT ]");
    } else {
        labels.push_back("[ VAULT (N/A) ]");
    }
    labels.push_back(WiFi.getMode() == WIFI_OFF ? "[ WI-FI: OFF ]" : "[ WI-FI: ON ]");
    for (auto& p : paths) {
        labels.push_back(_jm.getEntryTitle(p));
    }

    const int totalItems  = (int)labels.size();
    const int dispH       = _display.height();
    const int linesPerPage = (dispH - HEADER_H) / _itemH;

    int selected = 0;
    int topRow   = 0;
    bool needRedraw = true;
    bool fullRefreshPending = true;
    uint32_t lastActivity = millis();

    while (true) {
        _input.tick();

        // ── Idle timeout → sleep screen ───────────────────────────────────────
        if (millis() - lastActivity > IDLE_SLEEP_TIMEOUT_MS) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            _sleepScreen.sleep(0); // does not return
        }

        // ── Power-button deep sleep ───────────────────────────────────────────
        if (_input.isPowerButtonPressed()) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            _sleepScreen.sleep(0); // does not return
        }

        if (_input.wasUp()) {
            if (selected > 0) { selected--; needRedraw = true; }
            if (selected < topRow) topRow = selected;
            lastActivity = millis();
        }
        if (_input.wasDown()) {
            if (selected < totalItems - 1) { selected++; needRedraw = true; }
            if (selected >= topRow + linesPerPage) topRow = selected - linesPerPage + 1;
            lastActivity = millis();
        }
        if (_input.wasConfirm()) {
            lastActivity = millis();
            if (selected == 0) {
                return BrowseResult::NEW_ENTRY;
            } else if (selected == 1) {
                return BrowseResult::CALENDAR;
            } else if (selected == 2) {
                return BrowseResult::SEARCH;
            } else if (selected == 3) {
                return BrowseResult::VAULT_TOGGLE;
            } else if (selected == 3) {
                return BrowseResult::WIFI_TOGGLE;
            } else {
                outPath = paths[selected - FIXED_ITEMS];
                return BrowseResult::OPEN_ENTRY;
            }
        }
        if (_input.wasBack()) {
            lastActivity = millis();
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

    // ── Header ────────────────────────────────────────────────────────────────
    _display.drawText(fb, 4, 4, "JOURNAL", false, SCALE);

    // Current month label on the right side
    uint16_t year; uint8_t month;
    _clock.currentYearMonth(year, month);
    char monthBuf[12];
    snprintf(monthBuf, sizeof(monthBuf), "%04u-%02u", year, month);
    uint16_t monthLabelW = (uint16_t)(strlen(monthBuf) * _display.charAdvance(SCALE));
    _display.drawText(fb, dispW - monthLabelW - 4, 4, monthBuf, false, SCALE);

    // Today's writing prompt (scale 1, below the title row)
    struct tm now = _clock.now();
    const char* prompt = PromptPack::today(now);
    _display.drawText(fb, 4, 26, prompt, false, 1);

    // Separator line
    const uint16_t sepY = HEADER_H - 2;
    _display.fillRect(fb, 0, sepY, dispW, 1, true);

    // ── Entry list ────────────────────────────────────────────────────────────
    const int visibleCount = (int)labels.size();
    for (int i = 0; i < (int)((dispH - HEADER_H) / _itemH); i++) {
        int idx = topRow + i;
        if (idx >= visibleCount) break;

        uint16_t itemY = HEADER_H + (uint16_t)(i * _itemH);
        bool sel = (idx == selected);

        if (sel) {
            // Highlight bar
            _display.fillRect(fb, 0, itemY, dispW, _itemH, true);
        }
        _display.drawText(fb, 4, itemY + 2, labels[idx].c_str(), sel, SCALE);
    }
}
