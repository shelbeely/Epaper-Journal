// ─────────────────────────────────────────────────────────────────────────────
// X4Storage.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "X4Storage.h"
#include <stdio.h>

X4Storage::X4Storage() {}

bool X4Storage::init() {
    if (!SdMan.begin()) {
        X4_LOG(X4M_STORAGE_FAILED);
        return false;
    }
    _initialized = true;
    X4_LOG(X4M_STORAGE_OK);
    return true;
}

bool X4Storage::ready() const {
    return SdMan.ready();
}

bool X4Storage::ensureJournalPath(uint16_t year, uint8_t month) {
    char path[24];
    buildJournalPath(path, year, month);
    return SdMan.ensureDirectoryExists(path);
}

bool X4Storage::writeEntry(const char* path, const String& content) {
    return SdMan.writeFile(path, content);
}

String X4Storage::readEntry(const char* path) {
    return SdMan.readFile(path);
}

std::vector<String> X4Storage::listEntries(uint16_t year, uint8_t month) {
    char path[24];
    buildJournalPath(path, year, month);
    return SdMan.listFiles(path);
}

/*static*/
void X4Storage::buildJournalPath(char* buf, uint16_t year, uint8_t month) {
    snprintf(buf, 24, "/journal/%04u/%02u/", year, month);
}
