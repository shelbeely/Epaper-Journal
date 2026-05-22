#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// test/mocks/SDCardManager.h — stub replacing the community-sdk SDCardManager
//
// All methods are no-ops / return safe defaults so that project source files
// that depend on SDCardManager compile and link in the native test environment.
// ─────────────────────────────────────────────────────────────────────────────

#include "Arduino.h"
#include "SdFat.h"
#include <map>
#include <vector>
#include <string>
#include <map>

class SDCardManager {
public:
    static SDCardManager& getInstance() { return _instance; }

    // ── Test-control flags ────────────────────────────────────────────────────
    // Tests may set these to drive specific code paths in the modules under test.
    // All default to false so that existing tests are unaffected.
    static bool _stubReady;           // return value for ready()
    static bool _stubEnsureDir;       // return value for ensureDirectoryExists()
    static bool _stubWrite;           // return value for writeFile()
    static bool _stubRename;          // return value for rename()
    static bool _stubRemove;          // return value for remove()
    static String _stubReadContent;   // return value for readFile()
    static std::vector<String> _stubListFilesDefault;
    static std::map<std::string, std::vector<String>> _stubListFilesByPath;
    static std::map<std::string, String> _stubFileContents;
    static String _stubLastWritePath;
    static String _stubLastWriteContent;
    static int _stubWriteCallCount;
    static String _stubLastRenameSrc;
    static String _stubLastRenameDst;
    static int _stubRenameCallCount;
    static std::vector<String> _stubRemovedPaths;

    bool begin()        { return false; }
    bool ready() const  { return _stubReady; }

    std::vector<String> listFiles(const char* path = "/",
                                  int /*maxFiles*/     = 200) {
        const std::string key(path ? path : "");
        auto it = _stubListFilesByPath.find(key);
        if (it != _stubListFilesByPath.end()) return it->second;
        return _stubListFilesDefault;
    }
    String readFile(const char* path) {
        auto it = _stubFileContents.find(path ? path : "");
        return (it == _stubFileContents.end()) ? _stubReadContent : it->second;
    }
    bool   readFileToStream(const char* /*path*/, Print& /*out*/,
                            size_t /*chunkSize*/ = 256) { return false; }
    size_t readFileToBuffer(const char* path, char* buf,
                            size_t bufSize, size_t maxBytes = 0) {
        if (!buf || bufSize == 0) return 0;
        String content = readFile(path);
        size_t n = content.length();
        if (maxBytes > 0 && n > maxBytes) n = maxBytes;
        if (n > bufSize - 1) n = bufSize - 1;
        if (n > 0) memcpy(buf, content.c_str(), n);
        buf[n] = '\0';
        return n;
    }
    bool writeFile(const char* path, const String& content) {
        _stubLastWritePath = String(path);
        _stubLastWriteContent = content;
        _stubWriteCallCount++;
        return _stubWrite;
    }
    bool ensureDirectoryExists(const char* /*path*/) { return _stubEnsureDir; }

    FsFile open(const char* /*path*/, oflag_t /*flag*/ = O_RDONLY) {
        return FsFile{};
    }
    bool mkdir(const char* /*path*/, bool /*pFlag*/ = true) { return false; }
    bool exists(const char* /*path*/) { return false; }
    bool remove(const char* path) {
        _stubRemovedPaths.push_back(String(path));
        return _stubRemove;
    }
    bool rmdir(const char* /*path*/)  { return false; }
    bool rename(const char* src, const char* dst) {
        _stubLastRenameSrc = String(src);
        _stubLastRenameDst = String(dst);
        _stubRenameCallCount++;
        return _stubRename;
    }

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
inline bool   SDCardManager::_stubRename       = true;
inline bool   SDCardManager::_stubRemove       = false;
inline String SDCardManager::_stubReadContent;
inline std::vector<String> SDCardManager::_stubListFilesDefault;
inline std::map<std::string, std::vector<String>> SDCardManager::_stubListFilesByPath;
inline std::map<std::string, String> SDCardManager::_stubFileContents;
inline String SDCardManager::_stubLastWritePath;
inline String SDCardManager::_stubLastWriteContent;
inline int SDCardManager::_stubWriteCallCount = 0;
inline String SDCardManager::_stubLastRenameSrc;
inline String SDCardManager::_stubLastRenameDst;
inline int SDCardManager::_stubRenameCallCount = 0;
inline std::vector<String> SDCardManager::_stubRemovedPaths;

// Match the macro from the real SDCardManager.h
#define SdMan SDCardManager::getInstance()
