#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// JournalFrontmatter.h — Parse / serialize the YAML-style frontmatter block
//
// Supported format:
//   ---
//   title: My Entry Title
//   date: 2026-05-20 10:30:00
//   tags: personal, diary
//   ---
//   Body text here
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include "JournalEntry.h"

struct JournalFrontmatter {
    // Parse `fileContent` into `out`. Fields not present are left as empty strings.
    static void parse(const String& fileContent, JournalEntry& out) {
        out.title   = "";
        out.date    = "";
        out.tagsRaw = "";
        out.body    = "";

        // Must start with "---\n"
        if (!fileContent.startsWith("---\n")) {
            out.body = fileContent;
            return;
        }

        // Find closing "---\n" (or "---" at end of file)
        int endFm = fileContent.indexOf("\n---\n", 4);
        if (endFm < 0) {
            // Try "---" at very end of file
            int altEnd = fileContent.indexOf("\n---", 4);
            if (altEnd < 0) {
                out.body = fileContent;
                return;
            }
            endFm = altEnd;
        }

        // Parse frontmatter key-value lines
        String fm = fileContent.substring(4, endFm);
        int lineStart = 0;
        while (lineStart < (int)fm.length()) {
            int lineEnd = fm.indexOf('\n', lineStart);
            if (lineEnd < 0) lineEnd = fm.length();
            String line = fm.substring(lineStart, lineEnd);
            int colon = line.indexOf(": ");
            if (colon > 0) {
                String key = line.substring(0, colon);
                String val = line.substring(colon + 2);
                if (key == "title")   out.title   = val;
                else if (key == "date")  out.date    = val;
                else if (key == "tags")  out.tagsRaw = val;
            }
            lineStart = lineEnd + 1;
        }

        // Body follows the closing "---\n"
        int bodyStart = endFm + 5; // skip "\n---\n"
        if (bodyStart <= (int)fileContent.length()) {
            out.body = fileContent.substring(bodyStart);
        }
    }

    // Serialize `entry` back to a complete Markdown file with frontmatter.
    static String serialize(const JournalEntry& entry) {
        String result = "---\n";
        result += "title: " + entry.title + "\n";
        result += "date: "  + entry.date  + "\n";
        if (!entry.tagsRaw.isEmpty()) {
            result += "tags: " + entry.tagsRaw + "\n";
        }
        result += "---\n";
        result += entry.body;
        return result;
    }
};
