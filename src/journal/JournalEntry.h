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
};
