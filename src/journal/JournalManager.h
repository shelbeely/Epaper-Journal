#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// JournalManager.h — High-level journal operations wrapping X4Storage
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <vector>
#include "../storage/X4Storage.h"
#include "../system/X4Clock.h"
#include "../vault/VaultManager.h"
#include "JournalEntry.h"

class JournalManager {
public:
    // `vault` may be nullptr — pass a VaultManager to enable encryption.
    JournalManager(X4Storage& storage, X4Clock& clock,
                   VaultManager* vault = nullptr);

    // Startup hook for journal housekeeping.
    void begin();

    // Create a new entry with the given title.
    // Auto-generates a filename from the current timestamp.
    // Encrypts the file if the vault is unlocked.
    // Returns the full path on success, empty string on failure.
    String createEntry(const String& title);

    // Save an entry (serialize frontmatter + body) to `path` on SD.
    // Preserves encryption: re-encrypts if the existing file is encrypted and
    // the vault is unlocked. Returns false if the file is encrypted but the
    // vault is locked.
    bool saveEntry(const String& path, const JournalEntry& entry);

    // Load an entry from `path`. Populates `out`. Returns false on failure.
    // If the file is encrypted and the vault is locked, `out.locked` is true.
    bool loadEntry(const String& path, JournalEntry& out);

    // Read the raw file content of an entry (for HTTP export).
    String readEntryRaw(const String& path);

    // Read an entry for backup/export. Encrypted entries are returned decrypted
    // when possible unless `rawEncrypted` is true, in which case the on-disk
    // ciphertext is preserved.
    bool readEntryForExport(const String& path, String& out, bool rawEncrypted = false);

    // List entry paths for the given year/month, sorted alphabetically
    // (chronological because filenames are timestamp-based).
    std::vector<String> listEntries(uint16_t year, uint8_t month);

    // List all entry paths across every year/month stored on the SD card.
    // Returns paths sorted oldest-first (ascending). Searches 2020 → current year.
    std::vector<String> listAllPaths();

    // Search entry titles + first 500 chars of body for `query`, case-insensitive.
    // Locked/encrypted entries are matched by filename/date only.
    std::vector<String> searchEntries(const String& query);

    // Delete an entry by its full path. Returns false on failure.
    bool deleteEntry(const String& path);

    // Extract the entry title from frontmatter without loading the full body.
    // Falls back to a date extracted from the filename on failure.
    String getEntryTitle(const String& path);

    // Build a human-readable label from a filename like "20260520-103000.md"
    // → "2026-05-20 10:30"
    static String labelFromFilename(const String& filename);

private:
    X4Storage&    _storage;
    X4Clock&      _clock;
    VaultManager* _vault;  // nullable

    // Build "/journal/YYYY/MM/" directory path into buf (≥ 24 chars).
    static void _journalDir(char* buf, uint16_t year, uint8_t month);

    void cleanupOrphanTempFiles();
};
