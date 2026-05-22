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

    // ── Test-control flags ────────────────────────────────────────────────────
    // Tests may set these to drive specific code paths in the modules under test.
    // All default to false so that existing tests are unaffected.
    static bool _stubReady;           // return value for ready()
    static bool _stubEnsureDir;       // return value for ensureDirectoryExists()
    static bool _stubWrite;           // return value for writeFile()
    static String _stubReadContent;   // return value for readFile()
    static String _stubLastWritePath;
    static String _stubLastWriteContent;

    bool begin()        { return false; }
    bool ready() const  { return _stubReady; }

    std::vector<String> listFiles(const char* /*path*/ = "/",
                                  int /*maxFiles*/     = 200) {
        return {};
    }
    String readFile(const char* /*path*/) { return _stubReadContent; }
    bool   readFileToStream(const char* /*path*/, Print& /*out*/,
                            size_t /*chunkSize*/ = 256) { return false; }
    size_t readFileToBuffer(const char* /*path*/, char* /*buf*/,
                            size_t /*bufSize*/, size_t /*maxBytes*/ = 0) {
        return 0;
    }
    bool writeFile(const char* path, const String& content) {
        _stubLastWritePath = path ? path : "";
        _stubLastWriteContent = content;
        return _stubWrite;
    }
    bool ensureDirectoryExists(const char* /*path*/) { return _stubEnsureDir; }

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

// Stub control flags — all default to false so pre-existing tests are unaffected.
inline bool   SDCardManager::_stubReady        = false;
inline bool   SDCardManager::_stubEnsureDir    = false;
inline bool   SDCardManager::_stubWrite        = false;
inline String SDCardManager::_stubReadContent;
inline String SDCardManager::_stubLastWritePath;
inline String SDCardManager::_stubLastWriteContent;

// Match the macro from the real SDCardManager.h
#define SdMan SDCardManager::getInstance()
