#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// test/mocks/SdFat.h — stub for the SdFat library (used by SDCardManager.h)
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>

using oflag_t = uint8_t;

// Minimal file-handle stub
struct FsFile {
    bool isOpen() const { return false; }
    void close() {}
};

// oflag values referenced by SDCardManager
#define O_RDONLY  0x00
#define O_WRONLY  0x01
#define O_RDWR    0x02
#define O_CREAT   0x08
#define O_TRUNC   0x10
