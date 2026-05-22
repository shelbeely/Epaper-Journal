// ─────────────────────────────────────────────────────────────────────────────
// SleepScreen.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "SleepScreen.h"
#include "ThemeScreen.h"
#include <esp_sleep.h>
#include <Preferences.h>
#include <SDCardManager.h>
#include <string.h>
#include <stdio.h>

static constexpr const char* SLEEP_NS       = "sleep";
static constexpr const char* SLEEP_MODE_KEY = "mode";
static constexpr const char* SLEEP_PATH_KEY = "img_path";
static constexpr const char* SLEEP_DIR_KEY  = "img_dir";
static constexpr const char* SLEEP_IDX_KEY  = "img_idx";

SleepScreen::SleepScreen(X4Display& display, X4Clock& clock)
    : _display(display), _clock(clock)
{}

void SleepScreen::setThemeScreen(ThemeScreen* ts) {
    _themeScreen = ts;
}

void SleepScreen::loadConfig() {
    Preferences prefs;
    prefs.begin(SLEEP_NS, true);
    _mode = (SleepMode)prefs.getUChar(SLEEP_MODE_KEY, (uint8_t)SleepMode::CLOCK);
    if (_mode > SleepMode::THEME_COVER) _mode = SleepMode::CLOCK;
    String p = prefs.getString(SLEEP_PATH_KEY, "");
    if (!p.isEmpty()) _imagePath = p;
    String d = prefs.getString(SLEEP_DIR_KEY, "/sleep");
    if (!d.isEmpty()) _imageDir = d;
    prefs.end();
}

// ── Public entry point ────────────────────────────────────────────────────────

[[noreturn]] void SleepScreen::sleep(uint8_t batteryPct) {
    bool rendered = false;

    switch (_mode) {
    case SleepMode::BMP_FILE:
        if (!_imagePath.isEmpty()) {
            rendered = _renderBmpFile(_imagePath.c_str());
        }
        break;
    case SleepMode::BMP_FOLDER:
        rendered = _renderBmpFolder();
        break;
    case SleepMode::THEME_COVER:
        if (_themeScreen) {
            _renderThemeCover();
            rendered = true;
        }
        break;
    default:
        break;
    }

    if (!rendered) {
        _renderClock(batteryPct);
    }

    _display.sleep();
    esp_deep_sleep_start();
    __builtin_unreachable();
}

// ── CLOCK mode ────────────────────────────────────────────────────────────────

void SleepScreen::_renderClock(uint8_t batteryPct) {
    uint16_t dispW = _display.width();
    uint16_t dispH = _display.height();

    _display.clearFrameGrayscale();

    // ── Date / time (centered) ────────────────────────────────────────────────
    struct tm now = _clock.now();
    char timeBuf[9];   // "HH:MM:SS"
    char dateBuf[12];  // "YYYY-MM-DD"
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
             now.tm_hour, now.tm_min, now.tm_sec);
    snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d",
             now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);

    constexpr uint8_t TSCALE = 3;
    constexpr uint8_t DSCALE = 2;

    uint16_t timeW = (uint16_t)(strlen(timeBuf) * _display.charAdvance(TSCALE));
    uint16_t dateW = (uint16_t)(strlen(dateBuf) * _display.charAdvance(DSCALE));

    uint16_t timeY = (uint16_t)(dispH / 2 - _display.lineHeight(TSCALE) - 4);
    uint16_t dateY = (uint16_t)(dispH / 2 + 4);

    // Subtle rules above/below the time block
    uint16_t ruleY1 = (uint16_t)(timeY - 6);
    uint16_t ruleY2 = (uint16_t)(dateY + _display.lineHeight(DSCALE) + 4);
    _display.fillRectGray(0, ruleY1, dispW, 1, GrayLevel::DARK_GRAY);
    _display.fillRectGray(0, ruleY2, dispW, 1, GrayLevel::DARK_GRAY);

    _display.drawTextGray((dispW - timeW) / 2, timeY, timeBuf,
                          GrayLevel::BLACK, GrayLevel::WHITE, TSCALE);
    _display.drawTextGray((dispW - dateW) / 2, dateY, dateBuf,
                          GrayLevel::BLACK, GrayLevel::WHITE, DSCALE);

    // ── "ZZZ" bottom-right ────────────────────────────────────────────────────
    constexpr uint8_t ZSCALE = 2;
    uint16_t zW = (uint16_t)(3 * _display.charAdvance(ZSCALE));
    uint16_t zY = (uint16_t)(dispH - _display.lineHeight(ZSCALE) - 8);
    _display.drawTextGray(dispW - zW - 8, zY, "ZZZ",
                          GrayLevel::DARK_GRAY, GrayLevel::WHITE, ZSCALE);

    // ── Battery (top-right corner) ────────────────────────────────────────────
    if (batteryPct > 0) {
        char batBuf[8];
        snprintf(batBuf, sizeof(batBuf), "%u%%", batteryPct);
        uint16_t batW = (uint16_t)(strlen(batBuf) * _display.charAdvance(2));
        _display.drawTextGray(dispW - batW - 4, 4, batBuf,
                              GrayLevel::BLACK, GrayLevel::WHITE, 2);
    }

    _display.displayGrayscale();
}

// ── BMP_FILE mode ─────────────────────────────────────────────────────────────

bool SleepScreen::_renderBmpFile(const char* path) {
    _display.clearFrameGrayscale();
    if (!_blitBmp(path)) return false;
    _display.displayGrayscale();
    return true;
}

// ── BMP_FOLDER mode ───────────────────────────────────────────────────────────

bool SleepScreen::_renderBmpFolder() {
    // List .bmp files in the configured directory
    auto files = SdMan.listFiles(_imageDir.c_str());

    // Filter to only .bmp files
    std::vector<String> bmps;
    for (const auto& f : files) {
        String lc = f;
        lc.toLowerCase();
        if (lc.endsWith(".bmp")) bmps.push_back(f);
    }

    if (bmps.empty()) return false;

    // Read and advance slide index in NVS
    Preferences prefs;
    prefs.begin(SLEEP_NS, false);
    uint32_t idx = prefs.getUInt(SLEEP_IDX_KEY, 0);
    if (idx >= (uint32_t)bmps.size()) idx = 0;
    uint32_t nextIdx = (idx + 1) % (uint32_t)bmps.size();
    prefs.putUInt(SLEEP_IDX_KEY, nextIdx);
    prefs.end();

    // Build full path
    String path = _imageDir;
    if (!path.endsWith("/")) path += "/";
    path += bmps[(size_t)idx];

    _display.clearFrameGrayscale();
    if (!_blitBmp(path.c_str())) return false;
    _display.displayGrayscale();
    return true;
}

// ── THEME_COVER mode ──────────────────────────────────────────────────────────

void SleepScreen::_renderThemeCover() {
    DisplayOrientation oldOrientation = _display.getOrientation();
    _themeScreen->renderOnce();
    _display.displayGrayscale();
    (void)oldOrientation; // orientation stays PORTRAIT_CW for the sleep image
}

// ── 1-bit BMP loader ──────────────────────────────────────────────────────────
//
// Supports Windows Device-Independent Bitmap (DIB) with BITMAPINFOHEADER (40
// bytes).  Only 1 bit-per-pixel, uncompressed (BI_RGB).  Both bottom-up (the
// default) and top-down (negative height) rasters are accepted.  The 2-entry
// colour table is read to correctly map bit 0/bit 1 to WHITE/BLACK regardless
// of editor conventions.
//
// Maximum row stride buffered at a time: 128 bytes (covers 1024 pixels).
// Images wider than the logical display are clipped at the right edge;
// images taller than the logical display are clipped at the bottom.

uint32_t SleepScreen::_readU32LE(const uint8_t* p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

int32_t SleepScreen::_readS32LE(const uint8_t* p) {
    return (int32_t)_readU32LE(p);
}

uint16_t SleepScreen::_readU16LE(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

bool SleepScreen::_blitBmp(const char* path) {
    FsFile file;
    if (!SdMan.openFileForRead("SleepScreen", path, file)) return false;

    // ── Header: 14 byte file header + 40 byte DIB header + 8 byte colour table
    uint8_t hdr[62];
    if ((int)file.read(hdr, sizeof(hdr)) != (int)sizeof(hdr)) {
        file.close();
        return false;
    }

    // Magic check
    if (hdr[0] != 'B' || hdr[1] != 'M') { file.close(); return false; }

    uint32_t dataOffset = _readU32LE(hdr + 10);
    uint32_t dibSize    = _readU32LE(hdr + 14);
    int32_t  imgW       = _readS32LE(hdr + 18);
    int32_t  imgH       = _readS32LE(hdr + 22);
    uint16_t bpp        = _readU16LE(hdr + 28);
    uint32_t compress   = _readU32LE(hdr + 30);

    // Only accept 1-bit uncompressed DIB
    if (bpp != 1 || compress != 0 || dibSize < 40 || imgW <= 0 || imgH == 0) {
        file.close();
        return false;
    }

    bool     bottomUp = (imgH > 0);
    uint32_t absH     = (uint32_t)(imgH > 0 ? imgH : -imgH);
    uint32_t absW     = (uint32_t)imgW;

    // Row stride rounded up to 4-byte boundary (BMP spec)
    uint32_t rowStride = ((absW + 31u) / 32u) * 4u;

    // Determine fg/bg from the 2-entry colour table (BGR order at hdr[54..61])
    // Compute luminance for each entry; the darker one → BLACK
    uint16_t lum0 = (uint16_t)hdr[54] + hdr[55] + hdr[56]; // entry 0: B G R
    uint16_t lum1 = (uint16_t)hdr[58] + hdr[59] + hdr[60]; // entry 1: B G R
    // bit=1 → colour index 1. If entry 1 is darker, bit=1 → BLACK.
    GrayLevel bitOnLevel  = (lum1 <= lum0) ? GrayLevel::BLACK : GrayLevel::WHITE;
    GrayLevel bitOffLevel = (lum1 <= lum0) ? GrayLevel::WHITE : GrayLevel::BLACK;

    // Render dimensions — clip to logical display size
    uint16_t renderW = (uint16_t)(absW < (uint32_t)_display.width()
                                  ? absW : (uint32_t)_display.width());
    uint16_t renderH = (uint16_t)(absH < (uint32_t)_display.height()
                                  ? absH : (uint32_t)_display.height());

    uint8_t rowBuf[128]; // handles up to 128×8 = 1024 px wide images

    if (rowStride > sizeof(rowBuf)) { file.close(); return false; }

    for (uint16_t row = 0; row < renderH; row++) {
        // BMP rows are stored bottom-up by default
        uint32_t fileRow   = bottomUp ? (absH - 1u - row) : row;
        uint32_t offset    = dataOffset + fileRow * rowStride;

        if (!file.seekSet(offset)) break;
        if ((int)file.read(rowBuf, (size_t)rowStride) != (int)rowStride) break;

        _display.drawBitmapGray(0, row, renderW, 1,
                                rowBuf, (uint16_t)rowStride,
                                bitOnLevel, bitOffLevel);
    }

    file.close();
    return true;
}
