#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// test/mocks/SDCardManager.h — stub replacing the community-sdk SDCardManager
//
// All methods are no-ops / return safe defaults so that project source files
// that depend on SDCardManager compile and link in the native test environment.
// ─────────────────────────────────────────────────────────────────────────────

#include "Arduino.h"
#include "SdFat.h"
#include <vector>
#include <string>

class SDCardManager {
public:
    static SDCardManager& getInstance() { return _instance; }

    bool begin()        { return false; }
    bool ready() const  { return false; }

    std::vector<String> listFiles(const char* /*path*/ = "/",
                                  int /*maxFiles*/     = 200) {
        return {};
    }
    String readFile(const char* /*path*/) { return String(""); }
    bool   readFileToStream(const char* /*path*/, Print& /*out*/,
                            size_t /*chunkSize*/ = 256) { return false; }
    size_t readFileToBuffer(const char* /*path*/, char* /*buf*/,
                            size_t /*bufSize*/, size_t /*maxBytes*/ = 0) {
        return 0;
    }
    bool writeFile(const char* /*path*/, const String& /*content*/) {
        return false;
    }
    bool ensureDirectoryExists(const char* /*path*/) { return false; }

    FsFile open(const char* /*path*/, oflag_t /*flag*/ = O_RDONLY) {
        return FsFile{};
    }
    bool mkdir(const char* /*path*/, bool /*pFlag*/ = true) { return false; }
    bool exists(const char* /*path*/) { return false; }
    bool remove(const char* /*path*/) { return false; }
    bool rmdir(const char* /*path*/)  { return false; }
    bool rename(const char* /*src*/, const char* /*dst*/) { return false; }

    bool openFileForRead(const char* /*mod*/, const char* /*path*/,
                         FsFile& /*f*/) { return false; }
    bool openFileForRead(const char* /*mod*/, const std::string& /*path*/,
                         FsFile& /*f*/) { return false; }
    bool openFileForRead(const char* /*mod*/, const String& /*path*/,
                         FsFile& /*f*/) { return false; }
    bool openFileForWrite(const char* /*mod*/, const char* /*path*/,
                          FsFile& /*f*/) { return false; }
    bool openFileForWrite(const char* /*mod*/, const std::string& /*path*/,
                          FsFile& /*f*/) { return false; }
    bool openFileForWrite(const char* /*mod*/, const String& /*path*/,
                          FsFile& /*f*/) { return false; }
    bool removeDir(const char* /*path*/) { return false; }

private:
    SDCardManager() = default;
    static SDCardManager _instance;
};

// Definition of the static singleton (C++17 inline not available in all
// compilers; define once in this header via the usual ODR trick — each TU
// that includes this header sees an extern declaration, and we rely on the
// fact that native tests each compile as a single translation unit).
inline SDCardManager SDCardManager::_instance;

// Match the macro from the real SDCardManager.h
#define SdMan SDCardManager::getInstance()
