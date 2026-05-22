// ─────────────────────────────────────────────────────────────────────────────
// JournalExport.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "JournalExport.h"

namespace JournalExport {

String zipPathFromEntryPath(const String& path) {
    size_t start = 0;
    while (start < path.length() && path[start] == '/') start++;
    String out = path.substring(start);
    return out.isEmpty() ? "journal/entry.md" : out;
}

String selectExportContent(const String& rawContent,
                           VaultManager* vault,
                           bool exportEncrypted) {
    if (exportEncrypted || rawContent.isEmpty()) return rawContent;

    if (vault && vault->isUnlocked() &&
        VaultManager::isEncryptedContent(rawContent)) {
        String decrypted = vault->decrypt(rawContent);
        if (!decrypted.isEmpty()) return decrypted;
    }

    return rawContent;
}

String backupFilename(time_t now) {
    struct tm tmValue {};
#if defined(_WIN32)
    struct tm* tmp = localtime(&now);
    if (tmp) tmValue = *tmp;
#else
    if (localtime_r(&now, &tmValue) == nullptr) {
        tmValue = {};
    }
#endif

    char buf[64];
    snprintf(buf, sizeof(buf), "journal-backup-%04d%02d%02d.zip",
             tmValue.tm_year + 1900,
             tmValue.tm_mon + 1,
             tmValue.tm_mday);
    return String(buf);
}

uint32_t crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 1u) ? ((crc >> 1u) ^ 0xEDB88320u) : (crc >> 1u);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

} // namespace JournalExport
