// ─────────────────────────────────────────────────────────────────────────────
// JournalManager.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "JournalManager.h"
#include "JournalFrontmatter.h"
#include <algorithm>

JournalManager::JournalManager(X4Storage& storage, X4Clock& clock)
    : _storage(storage), _clock(clock)
{}

String JournalManager::createEntry(const String& title) {
    if (!_storage.ready()) return "";

    uint16_t year; uint8_t month;
    _clock.currentYearMonth(year, month);

    if (!_storage.ensureJournalPath(year, month)) return "";

    char dir[24];
    _journalDir(dir, year, month);

    String filename = _clock.nowFilestamp() + ".md";
    String path = String(dir) + filename;

    JournalEntry e;
    e.title = title.isEmpty() ? "New Entry" : title;
    e.date  = _clock.nowIso();
    e.body  = "# " + e.title + "\n\n";

    if (!saveEntry(path, e)) return "";
    return path;
}

bool JournalManager::saveEntry(const String& path, const JournalEntry& entry) {
    String content = JournalFrontmatter::serialize(entry);
    return _storage.writeEntry(path.c_str(), content);
}

bool JournalManager::loadEntry(const String& path, JournalEntry& out) {
    String content = _storage.readEntry(path.c_str());
    if (content.isEmpty()) return false;
    JournalFrontmatter::parse(content, out);
    return true;
}

String JournalManager::readEntryRaw(const String& path) {
    return _storage.readEntry(path.c_str());
}

std::vector<String> JournalManager::listEntries(uint16_t year, uint8_t month) {
    // Get filenames from storage (just the names, not full paths)
    std::vector<String> names = _storage.listEntries(year, month);

    char dir[24];
    _journalDir(dir, year, month);
    String dirStr(dir);

    // Build full paths
    std::vector<String> paths;
    paths.reserve(names.size());
    for (auto& name : names) {
        if (name.endsWith(".md")) {
            paths.push_back(dirStr + name);
        }
    }

    // Sort alphabetically (= chronologically for our timestamp filenames)
    std::sort(paths.begin(), paths.end());
    return paths;
}

bool JournalManager::deleteEntry(const String& path) {
    return _storage.deleteEntry(path.c_str());
}

String JournalManager::getEntryTitle(const String& path) {
    // Read only the first 256 bytes to extract the title efficiently
    char buf[257];
    size_t n = _storage.raw().readFileToBuffer(path.c_str(), buf, sizeof(buf), 250);
    if (n == 0) return labelFromFilename(path);

    String partial(buf, n);
    if (!partial.startsWith("---\n")) return labelFromFilename(path);

    int titlePos = partial.indexOf("title: ");
    if (titlePos < 0) return labelFromFilename(path);

    int lineEnd = partial.indexOf('\n', titlePos + 7);
    if (lineEnd < 0) lineEnd = (int)partial.length();
    String title = partial.substring(titlePos + 7, lineEnd);
    title.trim();
    return title.isEmpty() ? labelFromFilename(path) : title;
}

/*static*/
String JournalManager::labelFromFilename(const String& path) {
    // Extract just the filename from the path
    int slash = path.lastIndexOf('/');
    String filename = (slash >= 0) ? path.substring(slash + 1) : path;
    // Expected format: YYYYMMDD-HHMMSS.md  (15 chars before ".md")
    if (filename.length() < 15) return filename;
    // "20260520-103000" → "2026-05-20 10:30"
    char label[20];
    snprintf(label, sizeof(label),
             "%c%c%c%c-%c%c-%c%c %c%c:%c%c",
             filename[0], filename[1], filename[2], filename[3],    // YYYY
             filename[4], filename[5],                               // MM
             filename[6], filename[7],                               // DD
             filename[9], filename[10],                              // HH
             filename[11], filename[12]);                            // MM
    return String(label);
}

/*static*/
void JournalManager::_journalDir(char* buf, uint16_t year, uint8_t month) {
    snprintf(buf, 24, "/journal/%04u/%02u/", year, month);
}
