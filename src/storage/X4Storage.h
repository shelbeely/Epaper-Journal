#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// X4Storage.h — thin wrapper around SDCardManager (community-sdk)
//
// Provides journal-path helpers on top of the SdMan singleton.
// ─────────────────────────────────────────────────────────────────────────────

#include <SDCardManager.h>
#include "../config.h"
#include "../diagnostics/X4Log.h"

class X4Storage {
public:
    X4Storage();

    // Initialize the SD card. Returns true on success.
    bool init();

    // True if the SD card is mounted and ready.
    bool ready() const;

    // Ensure /journal/YYYY/MM/ path exists; returns false on failure.
    bool ensureJournalPath(uint16_t year, uint8_t month);

    // Write a journal entry. path should be absolute (e.g. /journal/2026/05/...).
    bool writeEntry(const char* path, const String& content);

    // Read a journal entry. Returns empty String on failure.
    String readEntry(const char* path);

    // List entries under /journal/YYYY/MM/
    std::vector<String> listEntries(uint16_t year, uint8_t month);

    // Delete an entry at path. Returns false on failure.
    bool deleteEntry(const char* path);

    // Expose raw SdMan for direct access when needed
    SDCardManager& raw() { return SdMan; }

private:
    bool _initialized = false;
    // Builds "/journal/YYYY/MM/" into buf (must be at least 20 chars)
    static void buildJournalPath(char* buf, uint16_t year, uint8_t month);
};
