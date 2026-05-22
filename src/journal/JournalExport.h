#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// JournalExport.h — shared helpers for journal backup exports
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <time.h>
#include "../vault/VaultManager.h"

namespace JournalExport {

// Convert an absolute journal path like "/journal/2026/05/entry.md" into the
// relative ZIP path stored inside the archive.
String zipPathFromEntryPath(const String& path);

// Choose the on-wire export content for a journal entry.
// - exportEncrypted = true  → return the raw on-disk content unchanged
// - exportEncrypted = false → decrypt vault entries when the vault is unlocked
//                              and decryption succeeds; otherwise preserve the
//                              raw content so backups remain complete.
String selectExportContent(const String& rawContent,
                           VaultManager* vault,
                           bool exportEncrypted);

// Build the download filename for the backup archive.
String backupFilename(time_t now);

// Standard ZIP/PKZIP CRC-32 used for stored files in the archive.
uint32_t crc32(const uint8_t* data, size_t len);

} // namespace JournalExport
