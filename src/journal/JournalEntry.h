#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// JournalEntry.h — In-memory representation of a single journal entry
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>

struct JournalEntry {
    String title;       // frontmatter: title
    String date;        // frontmatter: date (ISO-8601 "YYYY-MM-DD HH:MM:SS")
    String tagsRaw;     // frontmatter: tags (raw comma-separated string)
    String body;        // Markdown body text (after the closing ---)

    // True when the on-disk file is encrypted but the vault is currently locked.
    // Callers should treat this as a read-only placeholder entry.
    bool locked = false;
};
