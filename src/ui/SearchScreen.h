#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// SearchScreen.h — On-device journal search UI
//
// Lets the user enter a query with the hardware buttons, then browse a
// scrollable result list. Returns the selected path, or "" on Back/cancel.
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <vector>
#include "../journal/JournalManager.h"
#include "../display/X4Display.h"
#include "../input/X4Input.h"
#include "../config.h"
#include "SleepScreen.h"

class SearchScreen {
public:
    SearchScreen(JournalManager& jm, X4Display& display,
                 X4Input& input, SleepScreen& sleepScreen);

    // Run the search flow and return the selected entry path.
    // Returns "" if the user presses Back / cancels.
    String run();

private:
    JournalManager& _jm;
    X4Display&      _display;
    X4Input&        _input;
    SleepScreen&    _sleepScreen;

    static constexpr uint8_t SCALE    = 2;
    static constexpr uint8_t HEADER_H = 44;
    static constexpr uint8_t ITEM_H   = FONT5X7_LINE_H * SCALE;
    static constexpr uint8_t QUERY_LEN = 12;

    static const char* CHARSET;

    static uint8_t _charsetLen();
    static String _queryFromSlots(const uint8_t slots[QUERY_LEN]);

    void _renderQuery(const uint8_t slots[QUERY_LEN], uint8_t cursor);
    void _renderResults(const String& query, const std::vector<String>& labels,
                        int selected, int topRow);
    String _labelForPath(const String& path);
};
