#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// BrowseScreen.h — On-device journal entry list UI
//
// Shows entries for the current month plus a "[ + NEW ENTRY ]" action.
// Up/Down scrolls; Confirm selects; Back exits.
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include "../journal/JournalManager.h"
#include "../display/X4Display.h"
#include "../input/X4Input.h"
#include "../system/X4Clock.h"

enum class BrowseResult {
    OPEN_ENTRY,   // user selected an existing entry
    NEW_ENTRY,    // user chose "[ + NEW ENTRY ]"
    BACK,         // user pressed Back / no action
};

class BrowseScreen {
public:
    BrowseScreen(JournalManager& jm, X4Display& display,
                 X4Input& input, X4Clock& clock);

    // Run the browse UI (blocking). Returns the result code.
    // On OPEN_ENTRY, `outPath` is filled with the selected entry path.
    // On NEW_ENTRY / BACK, `outPath` is empty.
    BrowseResult run(String& outPath);

private:
    JournalManager& _jm;
    X4Display&      _display;
    X4Input&        _input;
    X4Clock&        _clock;

    static constexpr uint8_t SCALE      = 2;
    static constexpr uint8_t HEADER_H   = 44; // pixels reserved for header area
    static constexpr uint8_t ITEM_H     = FONT5X7_LINE_H * SCALE; // 18px per item

    void _render(const std::vector<String>& labels,
                 int selected, int topRow);
};
