#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// BrowseScreen.h — On-device journal entry list UI
//
// Shows entries for the current month plus "[ + NEW ENTRY ]",
// "[ STREAK ]", and a vault-lock/unlock item. Up/Down scrolls;
// Confirm selects; Back exits.
// Displays today's writing prompt in the header.
// Auto-sleeps after IDLE_SLEEP_TIMEOUT_MS of inactivity.
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include "../journal/JournalManager.h"
#include "../display/X4Display.h"
#include "../input/X4Input.h"
#include "../system/X4Clock.h"
#include "../vault/VaultManager.h"
#include "../config.h"
#include "SleepScreen.h"

enum class BrowseResult {
    OPEN_ENTRY,    // user selected an existing entry
    NEW_ENTRY,     // user chose "[ + NEW ENTRY ]"
    CALENDAR,      // user chose "[ STREAK ]"
    VAULT_TOGGLE,  // user chose "[ LOCK VAULT ]" or "[ UNLOCK VAULT ]"
    WIFI_TOGGLE,   // user chose "[ WIFI: ON ]" or "[ WIFI: OFF ]"
    BACK,          // user pressed Back / no action
};

class BrowseScreen {
public:
    BrowseScreen(JournalManager& jm, X4Display& display,
                 X4Input& input, X4Clock& clock, SleepScreen& sleepScreen,
                 VaultManager* vault = nullptr);

    // Run the browse UI (blocking). Returns the result code.
    // On OPEN_ENTRY, `outPath` is filled with the selected entry path.
    // On NEW_ENTRY / CALENDAR / VAULT_TOGGLE / WIFI_TOGGLE / BACK, `outPath` is empty.
    BrowseResult run(String& outPath);

private:
    JournalManager& _jm;
    X4Display&      _display;
    X4Input&        _input;
    X4Clock&        _clock;
    SleepScreen&    _sleepScreen;
    VaultManager*   _vault;

    static constexpr uint8_t  SCALE       = 2;
    // Header area: title row + prompt row + padding (fixed layout).
    static constexpr uint8_t  HEADER_H    = 44;
    // Number of fixed items before the entry list
    static constexpr uint8_t  FIXED_ITEMS = 4;  // NEW_ENTRY + STREAK + VAULT + WIFI

    uint16_t _itemH;  ///< Line height of the active font at SCALE (runtime)

    void _render(const std::vector<String>& labels,
                 int selected, int topRow);
};
