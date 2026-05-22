// ─────────────────────────────────────────────────────────────────────────────
// JournalManager.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "JournalManager.h"
#include "JournalFrontmatter.h"
#include <algorithm>
#include <ctype.h>

namespace {

String toLowerCopy(const String& input) {
    String out;
    for (unsigned int i = 0; i < input.length(); i++) {
        out += (char)tolower((unsigned char)input[i]);
    }
    return out;
}

bool containsCaseInsensitive(const String& haystack, const String& needleLower) {
    if (needleLower.isEmpty()) return false;
    return toLowerCopy(haystack).indexOf(needleLower) >= 0;
}

} // namespace

JournalManager::JournalManager(X4Storage& storage, X4Clock& clock,
                                VaultManager* vault)
    : _storage(storage), _clock(clock), _vault(vault)
{}

void JournalManager::begin() {
    cleanupOrphanTempFiles();
}

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

    // Check whether the existing on-disk file is encrypted
    bool existingIsEncrypted = false;
    if (_vault) {
        String existing = _storage.readEntry(path.c_str());
        existingIsEncrypted = !existing.isEmpty() &&
                              VaultManager::isEncryptedContent(existing);
    }

    // Encrypt if: (a) vault is unlocked AND entry is new, OR
    //             (b) the existing file was already encrypted
    if (_vault && _vault->isUnlocked() && existingIsEncrypted) {
        content = _vault->encrypt(content);
        if (content.isEmpty()) return false;
    } else if (_vault && _vault->isUnlocked() && !existingIsEncrypted) {
        // New entry created while vault is unlocked → encrypt it
        // Note: only when there is no existing file (path doesn't exist yet)
        if (!_storage.ready()) return false;
        content = _vault->encrypt(content);
        if (content.isEmpty()) return false;
    } else if (_vault && !_vault->isUnlocked() && existingIsEncrypted) {
        // Cannot update encrypted file without key
        return false;
    }
    // else: vault nullptr or entry is plaintext → save as-is

    String tmpPath = path + ".tmp";
    if (!_storage.writeEntry(tmpPath.c_str(), content)) return false;
    if (!_storage.raw().rename(tmpPath.c_str(), path.c_str())) {
        _storage.raw().remove(tmpPath.c_str());
        return false;
    }
    return true;
}

bool JournalManager::loadEntry(const String& path, JournalEntry& out) {
    String content = _storage.readEntry(path.c_str());
    if (content.isEmpty()) return false;

    if (_vault && VaultManager::isEncryptedContent(content)) {
        if (_vault->isUnlocked()) {
            String decrypted = _vault->decrypt(content);
            if (decrypted.isEmpty()) {
                // Wrong key or corrupt data
                out.title  = "DECRYPT FAILED";
                out.body   = "[Authentication failed — wrong PIN?]";
                out.locked = false;
                return true;
            }
            JournalFrontmatter::parse(decrypted, out);
            out.locked = false;
            if (content.startsWith(VaultManager::VAULT_HEADER_V1)) {
                String upgraded = _vault->encrypt(decrypted);
                if (!upgraded.isEmpty()) {
                    _storage.writeEntry(path.c_str(), upgraded);
                }
            }
        } else {
            out.title  = "[LOCKED]";
            out.body   = "[This entry is encrypted. Unlock the vault to read.]";
            out.locked = true;
        }
        return true;
    }

    JournalFrontmatter::parse(content, out);
    out.locked = false;
    return true;
}

String JournalManager::readEntryRaw(const String& path) {
    return _storage.readEntry(path.c_str());
}

bool JournalManager::readEntryForExport(const String& path, String& out, bool rawEncrypted) {
    out = _storage.readEntry(path.c_str());
    if (out.isEmpty()) return false;

    if (!_vault || !VaultManager::isEncryptedContent(out)) {
        return true;
    }

    if (rawEncrypted || !_vault->isUnlocked()) {
        return true;
    }

    String decrypted = _vault->decrypt(out);
    if (!decrypted.isEmpty()) {
        out = decrypted;
    }
    return true;
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

std::vector<String> JournalManager::listAllPaths() {
    std::vector<String> result;
    if (!_storage.ready()) return result;

    uint16_t curYear = _clock.effectiveCurrentYear();

    for (uint16_t y = 2020; y <= curYear + 1; y++) {
        for (uint8_t m = 1; m <= 12; m++) {
            auto paths = listEntries(y, m);
            for (auto& p : paths) result.push_back(p);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<String> JournalManager::searchEntries(const String& query) {
    std::vector<String> matches;

    String needle = query;
    needle.trim();
    if (needle.isEmpty()) return matches;
    String needleLower = toLowerCopy(needle);

    auto paths = listAllPaths();
    matches.reserve(paths.size());

    for (const auto& path : paths) {
        String searchText = path + "\n" + labelFromFilename(path);

        JournalEntry entry;
        if (loadEntry(path, entry) && !entry.locked) {
            searchText += "\n";
            searchText += entry.title;
            searchText += "\n";
            if (entry.body.length() > 500) {
                searchText += entry.body.substring(0, 500);
            } else {
                searchText += entry.body;
            }
        }

        if (containsCaseInsensitive(searchText, needleLower)) {
            matches.push_back(path);
        }
    }

    return matches;
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

    // Encrypted entries: show "[LOCKED]" as the title
    if (_vault && VaultManager::isEncryptedContent(partial)) {
        if (_vault->isUnlocked()) {
            // Need to decrypt full content to get title; use loadEntry path
            JournalEntry e;
            if (loadEntry(path, e)) return e.title;
        }
        return "[LOCKED]";
    }

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

void JournalManager::cleanupOrphanTempFiles() {
    if (!_storage.ready()) return;

    uint16_t curYear; uint8_t dummy;
    _clock.currentYearMonth(curYear, dummy);

    for (uint16_t y = 2020; y <= curYear + 1; y++) {
        for (uint8_t m = 1; m <= 12; m++) {
            char dir[24];
            _journalDir(dir, y, m);
            auto names = _storage.raw().listFiles(dir);
            for (const auto& name : names) {
                if (!name.endsWith(".tmp")) continue;
                String fullPath = String(dir) + name;
                _storage.raw().remove(fullPath.c_str());
            }
        }
    }
}
