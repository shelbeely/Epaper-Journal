// ─────────────────────────────────────────────────────────────────────────────
// X4Display.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "X4Display.h"
#include "BitmapFont.h"
#include <Arduino.h>
#include <string.h>

// Font data arrays are compiled only in FontData.cpp; access them here via the
// extern declarations from BitmapFont.h.

X4Display::X4Display()
    : _eink(EPD_SCLK, EPD_MOSI, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
{}

bool X4Display::init() {
    X4_DLOG(X4M_DISPLAY_INIT_START);
    _status.initialized = false;
    _status.lastError   = nullptr;

    _eink.begin();
    _initialized = true;
    _status.initialized = true;

    // Default font: FONT_5X7
    _activeFont = &FONT_5X7;

    // Allocate RED plane on heap (48 KB) to avoid overflowing static DRAM.
    if (!_grayPlane) {
        _grayPlane = (uint8_t*)malloc(_eink.getBufferSize());
        if (_grayPlane) memset(_grayPlane, 0xFF, _eink.getBufferSize());
    }

    X4_DLOG(X4M_DISPLAY_FRAMEBUFFER_ALLOC);
    X4_LOG(X4M_DISPLAY_INIT_OK);
    return _grayPlane != nullptr;
}

// ── Refresh helpers ───────────────────────────────────────────────────────────

void X4Display::fullRefresh() {
    X4_DLOG(X4M_DISPLAY_FULL_REFRESH_START);
    uint32_t t0 = millis();
    _fastRefreshCount = 0;
    _grayscaleActive = false;
    _eink.displayBuffer(EInkDisplay::FULL_REFRESH);
    _recordRefresh("full", millis() - t0);
    X4_LOG(X4M_DISPLAY_REFRESH_OK);
}

void X4Display::halfRefresh() {
    uint32_t t0 = millis();
    _fastRefreshCount = 0;
    _grayscaleActive = false;
    _eink.displayBuffer(EInkDisplay::HALF_REFRESH);
    _recordRefresh("half", millis() - t0);
    X4_LOG(X4M_DISPLAY_REFRESH_OK);
}

void X4Display::fastRefresh() {
    uint32_t t0 = millis();
#if GHOSTING_FULL_REFRESH_INTERVAL > 0
    if (++_fastRefreshCount >= GHOSTING_FULL_REFRESH_INTERVAL) {
        _fastRefreshCount = 0;
        _grayscaleActive = false;
        _eink.displayBuffer(EInkDisplay::FULL_REFRESH);
        _recordRefresh("full-ghost", millis() - t0);
    } else {
        _grayscaleActive = false;
        _eink.displayBuffer(EInkDisplay::FAST_REFRESH);
        _recordRefresh("fast", millis() - t0);
    }
#else
    _grayscaleActive = false;
    _eink.displayBuffer(EInkDisplay::FAST_REFRESH);
    _recordRefresh("fast", millis() - t0);
#endif
    X4_LOG(X4M_DISPLAY_REFRESH_OK);
}

void X4Display::partialRefresh(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    X4_DLOG(X4M_DISPLAY_PARTIAL_REFRESH_START);
    uint32_t t0 = millis();
    _eink.displayWindow(x, y, w, h);
    _recordRefresh("partial", millis() - t0);
    X4_LOG(X4M_DISPLAY_REFRESH_OK);
}

void X4Display::clear() {
    _grayscaleActive = false;
    _eink.clearScreen(0xFF);
    fullRefresh();
}

// ── Power management ──────────────────────────────────────────────────────────

void X4Display::sleep() {
    _eink.deepSleep();
    X4_DLOG(X4M_DISPLAY_SLEEP_OK);
}

void X4Display::wake() {
    _fastRefreshCount = 0;
    _eink.begin();
    X4_DLOG(X4M_DISPLAY_WAKE_OK);
}

// ── Framebuffer access ────────────────────────────────────────────────────────

uint8_t* X4Display::getFrameBuffer() {
    return _eink.getFrameBuffer();
}

uint16_t X4Display::width() const {
    if (_orientation != DisplayOrientation::LANDSCAPE) {
        return _physH();  // portrait logical width = physical height = 480
    }
    return _physW();      // landscape: 800
}

uint16_t X4Display::height() const {
    if (_orientation != DisplayOrientation::LANDSCAPE) {
        return _physW();  // portrait logical height = physical width = 800
    }
    return _physH();      // landscape: 480
}

// ── Orientation ───────────────────────────────────────────────────────────────

void X4Display::setOrientation(DisplayOrientation o) {
    _orientation = o;
}

// ── Font selection ────────────────────────────────────────────────────────────

void X4Display::setFont(const BitmapFont* font) {
    _activeFont = font ? font : &FONT_5X7;
}

uint16_t X4Display::charAdvance(uint8_t scale) const {
    return (uint16_t)(_activeFont->advance * scale);
}

uint16_t X4Display::lineHeight(uint8_t scale) const {
    return (uint16_t)(_activeFont->lineH * scale);
}

uint16_t X4Display::glyphHeight(uint8_t scale) const {
    return (uint16_t)(_activeFont->glyphH * scale);
}

// ── Test patterns ─────────────────────────────────────────────────────────────

void X4Display::renderAllWhite() {
    _eink.clearScreen(0xFF);
    fullRefresh();
}

void X4Display::renderAllBlack() {
    _eink.clearScreen(0x00);
    fullRefresh();
}

void X4Display::renderCheckerboard() {
    uint8_t* fb     = _eink.getFrameBuffer();
    uint16_t wBytes = _eink.getDisplayWidthBytes();
    uint16_t h      = _eink.getDisplayHeight();

    for (uint16_t row = 0; row < h; row++) {
        for (uint16_t col = 0; col < wBytes; col++) {
            uint8_t pattern = (col % 2 == 0) ? 0xAA : 0x55;
            if (row % 2 != 0) pattern = ~pattern;
            fb[row * wBytes + col] = pattern;
        }
    }
    fullRefresh();
}

void X4Display::renderBorderWithCornerLabels() {
    _eink.clearScreen(0xFF);
    uint8_t* fb     = _eink.getFrameBuffer();
    uint16_t wBytes = _eink.getDisplayWidthBytes();
    uint16_t w      = _eink.getDisplayWidth();
    uint16_t h      = _eink.getDisplayHeight();

    for (uint16_t col = 0; col < wBytes; col++) {
        fb[col] = 0x00;
        fb[(h - 1) * wBytes + col] = 0x00;
    }
    for (uint16_t row = 0; row < h; row++) {
        fb[row * wBytes] &= 0x7F;
        fb[row * wBytes + wBytes - 1] &= 0xFE;
    }
    auto markCorner = [&](uint16_t px, uint16_t py) {
        for (uint16_t dy = 0; dy < 4 && (py + dy) < h; dy++) {
            for (uint16_t dx = 0; dx < 4 && (px + dx) < w; dx++) {
                uint16_t byteIdx = (py + dy) * wBytes + (px + dx) / 8;
                uint8_t  bitMask = 0x80 >> ((px + dx) % 8);
                fb[byteIdx] &= ~bitMask;
            }
        }
    };
    markCorner(2,     2);
    markCorner(w - 6, 2);
    markCorner(2,     h - 6);
    markCorner(w - 6, h - 6);
    fullRefresh();
}

void X4Display::renderDiagonalLine() {
    _eink.clearScreen(0xFF);
    uint8_t* fb     = _eink.getFrameBuffer();
    uint16_t wBytes = _eink.getDisplayWidthBytes();
    uint16_t w      = _eink.getDisplayWidth();
    uint16_t h      = _eink.getDisplayHeight();

    for (uint16_t i = 0; i < h; i++) {
        uint16_t px = (uint32_t)i * (w - 1) / (h - 1);
        uint16_t byteIdx = i * wBytes + px / 8;
        uint8_t  bitMask = 0x80 >> (px % 8);
        fb[byteIdx] &= ~bitMask;
    }
    fullRefresh();
}

void X4Display::renderFontSample() {
    _eink.clearScreen(0xFF);
    uint8_t* fb     = _eink.getFrameBuffer();
    uint16_t wBytes = _eink.getDisplayWidthBytes();
    uint16_t h      = _eink.getDisplayHeight();

    auto drawBlock = [&](uint16_t bx, uint16_t by) {
        for (uint16_t row = by; row < by + 8 && row < h; row++) {
            fb[row * wBytes + bx] = 0x00;
        }
    };
    for (int i = 0; i < 7; i++) {
        drawBlock(10 + i * 10, 20);
    }
    fullRefresh();
}

void X4Display::renderPartialRefreshRect() {
    _eink.clearScreen(0xFF);
    uint8_t* fb     = _eink.getFrameBuffer();
    uint16_t wBytes = _eink.getDisplayWidthBytes();
    uint16_t w      = _eink.getDisplayWidth();
    uint16_t h      = _eink.getDisplayHeight();
    uint16_t rx = w / 4, ry = h / 4, rw = w / 2, rh = h / 2;

    for (uint16_t row = ry; row < ry + rh; row++) {
        for (uint16_t col = rx / 8; col < (rx + rw) / 8; col++) {
            fb[row * wBytes + col] = 0x00;
        }
    }
    _eink.displayWindow(rx, ry, rw, rh);
    _recordRefresh("partial", 0);
}

bool X4Display::renderTestPattern(const char* name) {
    if (strcmp(name, "all_white")            == 0) { renderAllWhite();             return true; }
    if (strcmp(name, "all_black")            == 0) { renderAllBlack();             return true; }
    if (strcmp(name, "checkerboard")         == 0) { renderCheckerboard();         return true; }
    if (strcmp(name, "border")               == 0) { renderBorderWithCornerLabels(); return true; }
    if (strcmp(name, "diagonal")             == 0) { renderDiagonalLine();         return true; }
    if (strcmp(name, "font_sample")          == 0) { renderFontSample();           return true; }
    if (strcmp(name, "partial_refresh_rect") == 0) { renderPartialRefreshRect();   return true; }
    return false;
}

// ── Private helpers ───────────────────────────────────────────────────────────

void X4Display::_recordRefresh(const char* type, uint32_t durationMs) {
    _status.lastRefreshType = type;
    _status.lastRefreshMs   = durationMs;
}

void X4Display::_setError(const char* msg) {
    _status.lastError = msg;
    X4_LOG(X4M_DISPLAY_INIT_FAILED);
}

// ─────────────────────────────────────────────────────────────────────────────
// _setPixel — orientation-aware single-pixel write
//
// (lx, ly) are logical coordinates in the current orientation's coordinate
// space.  The transform maps them to physical (px, py) in the 800 × 480
// framebuffer before writing the bit.
//
//   LANDSCAPE:    px = lx,         py = ly
//   PORTRAIT_CW:  px = phW-1-ly,   py = lx      (right edge up)
//   PORTRAIT_CCW: px = ly,         py = phH-1-lx (left edge up)
// ─────────────────────────────────────────────────────────────────────────────

void X4Display::_setPixel(uint8_t* fb, uint16_t lx, uint16_t ly, bool black) {
    uint16_t phW = _physW();
    uint16_t phH = _physH();
    uint16_t px, py;

    switch (_orientation) {
    case DisplayOrientation::PORTRAIT_CW:
        // logical: width=phH(480), height=phW(800)
        if (lx >= phH || ly >= phW) return;
        px = phW - 1u - ly;
        py = lx;
        break;
    case DisplayOrientation::PORTRAIT_CCW:
        // logical: width=phH(480), height=phW(800)
        if (lx >= phH || ly >= phW) return;
        px = ly;
        py = phH - 1u - lx;
        break;
    default: // LANDSCAPE
        if (lx >= phW || ly >= phH) return;
        px = lx;
        py = ly;
        break;
    }

    uint16_t wBytes  = phW / 8u;
    uint16_t byteIdx = py * wBytes + px / 8u;
    uint8_t  bitMask = 0x80u >> (px % 8u);
    if (black) fb[byteIdx] &= ~bitMask;
    else       fb[byteIdx] |=  bitMask;
}

// ── Font rendering ────────────────────────────────────────────────────────────

void X4Display::drawChar(uint8_t* fb, uint16_t x, uint16_t y, char c,
                          bool inverted, uint8_t scale) {
    if (!fb || scale == 0) return;
    const BitmapFont* font = _activeFont;
    if (!font) return;

    uint8_t fc = (uint8_t)c;
    if (fc < font->firstChar || fc >= font->firstChar + font->numChars) fc = (uint8_t)' ';

    int glyphIdx = (int)(fc - font->firstChar);
    const uint8_t* glyphData;
    if (font->columnMajor) {
        glyphData = font->data + glyphIdx * (int)font->glyphW;
    } else {
        glyphData = font->data + glyphIdx * (int)font->glyphH * (int)font->bytesPerRow;
    }

    for (uint8_t col = 0; col < font->advance; col++) {
        for (uint8_t row = 0; row < font->glyphH; row++) {
            bool pixOn;
            if (col < font->glyphW) {
                if (font->columnMajor) {
                    // Column-major: byte = column, bit 0 = top row
                    pixOn = (glyphData[col] >> row) & 0x01u;
                } else {
                    // Row-major: MSB of first byte = leftmost pixel
                    uint8_t byteInRow = col >> 3u;
                    uint8_t bitMask   = 0x80u >> (col & 7u);
                    pixOn = (glyphData[(int)row * (int)font->bytesPerRow + (int)byteInRow] & bitMask) != 0;
                }
            } else {
                pixOn = false; // gap column
            }
            bool drawBlack = inverted ? !pixOn : pixOn;
            if (!drawBlack && !inverted) continue;

            for (uint8_t sy = 0; sy < scale; sy++) {
                for (uint8_t sx = 0; sx < scale; sx++) {
                    _setPixel(fb,
                        x + (uint16_t)col * scale + sx,
                        y + (uint16_t)row * scale + sy,
                        drawBlack);
                }
            }
        }
    }
}

void X4Display::drawText(uint8_t* fb, uint16_t x, uint16_t y, const char* str,
                          bool inverted, uint8_t scale) {
    if (!fb || !str || scale == 0) return;
    const BitmapFont* font = _activeFont;
    if (!font) return;
    uint16_t cx      = x;
    uint16_t charAdv = (uint16_t)(font->advance * scale);
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') break;
        if (cx + charAdv > width()) break;
        drawChar(fb, cx, y, str[i], inverted, scale);
        cx += charAdv;
    }
}

uint16_t X4Display::drawTextWrapped(uint8_t* fb, uint16_t x, uint16_t startY,
                                     uint16_t maxW, const char* str,
                                     bool inverted, uint8_t scale) {
    if (!fb || !str || scale == 0 || maxW == 0) return startY;
    const BitmapFont* font = _activeFont;
    if (!font) return startY;
    uint16_t charAdv = (uint16_t)(font->advance * scale);
    uint16_t lineH   = (uint16_t)(font->lineH   * scale);
    uint16_t cx = x;
    uint16_t cy = startY;
    const char* p = str;

    while (*p) {
        if (*p == '\n') {
            cx = x;
            cy += lineH;
            p++;
            continue;
        }
        const char* wordEnd = p;
        while (*wordEnd && *wordEnd != ' ' && *wordEnd != '\n') wordEnd++;
        int wordLen = (int)(wordEnd - p);
        uint16_t wordW = (uint16_t)(wordLen * charAdv);

        if (cx + wordW > x + maxW && cx > x) {
            cx = x;
            cy += lineH;
        }
        for (int i = 0; i < wordLen; i++) {
            if (cx + charAdv > x + maxW) {
                cx = x;
                cy += lineH;
            }
            drawChar(fb, cx, cy, p[i], inverted, scale);
            cx += charAdv;
        }
        p = wordEnd;
        if (*p == ' ') {
            cx += charAdv;
            p++;
        }
    }
    return cy + lineH;
}

void X4Display::fillRect(uint8_t* fb, uint16_t x, uint16_t y,
                          uint16_t w, uint16_t h, bool black) {
    if (!fb || w == 0 || h == 0) return;

    if (_orientation == DisplayOrientation::LANDSCAPE) {
        // ── Fast path: direct byte-aligned writes (landscape only) ────────────
        uint16_t dispW  = _physW();
        uint16_t dispH  = _physH();
        uint16_t wBytes = dispW / 8;
        uint16_t xEnd   = (x + w < dispW) ? x + w : dispW;
        uint16_t yEnd   = (y + h < dispH) ? (uint16_t)(y + h) : dispH;
        uint8_t  fill   = black ? 0x00 : 0xFF;

        for (uint16_t py = y; py < yEnd; py++) {
            uint16_t px = x;
            // Leading unaligned pixels
            while (px < xEnd && (px % 8) != 0) {
                uint16_t byteIdx = py * wBytes + px / 8;
                uint8_t  bitMask = 0x80 >> (px % 8);
                if (black) fb[byteIdx] &= ~bitMask;
                else       fb[byteIdx] |=  bitMask;
                px++;
            }
            // Byte-aligned span
            uint16_t fullEnd = xEnd & ~(uint16_t)7u;
            if (fullEnd > px) {
                uint16_t startByte = px / 8;
                uint16_t byteCount = (fullEnd - px) / 8;
                memset(fb + py * wBytes + startByte, fill, byteCount);
                px = fullEnd;
            }
            // Trailing unaligned pixels
            while (px < xEnd) {
                uint16_t byteIdx = py * wBytes + px / 8;
                uint8_t  bitMask = 0x80 >> (px % 8);
                if (black) fb[byteIdx] &= ~bitMask;
                else       fb[byteIdx] |=  bitMask;
                px++;
            }
        }
    } else {
        // ── Portrait: pixel-by-pixel with orientation transform ───────────────
        for (uint16_t row = 0; row < h; row++) {
            for (uint16_t col = 0; col < w; col++) {
                _setPixel(fb, x + col, y + row, black);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Grayscale helpers and primitives
// ─────────────────────────────────────────────────────────────────────────────

// _setGrayPixel — writes one logical pixel to both the BW plane (SDK fb) and
// the RED plane (_grayPlane) according to the GrayLevel encoding:
//
//   Level       BW bit  RED bit
//   BLACK         0       0
//   DARK_GRAY     0       1
//   LIGHT_GRAY    1       0
//   WHITE         1       1
//
// _setPixel(fb, lx, ly, true)  → bit = 0  (black in BW plane)
// _setPixel(fb, lx, ly, false) → bit = 1  (white in BW plane / RED bit set)

void X4Display::_setGrayPixel(uint16_t lx, uint16_t ly, GrayLevel level) {
    if (!_grayPlane) return;
    uint8_t v      = (uint8_t)level;
    bool    bwBit  = (v >> 1) & 1u;  // bit 1 → BW plane
    bool    redBit = v & 1u;          // bit 0 → RED plane
    _setPixel(_eink.getFrameBuffer(), lx, ly, !bwBit);
    _setPixel(_grayPlane,             lx, ly, !redBit);
}

void X4Display::clearFrameGrayscale() {
    uint32_t sz = _eink.getBufferSize();
    memset(_eink.getFrameBuffer(), 0xFF, sz);   // BW plane: all 1 = white
    if (_grayPlane) memset(_grayPlane, 0xFF, sz); // RED plane: all 1 = WHITE
}

void X4Display::displayGrayscale() {
    if (!_grayPlane) return;
    uint32_t t0     = millis();
    uint8_t* sdk_fb = _eink.getFrameBuffer();
    // Write BW plane to hardware BW RAM (CMD 0x24)
    _eink.copyGrayscaleLsbBuffers(sdk_fb);
    // Write RED plane to hardware RED RAM (CMD 0x26)
    _eink.copyGrayscaleMsbBuffers(_grayPlane);
    // Load grayscale LUT and trigger refresh
    _eink.displayGrayBuffer();
#ifdef EINK_DISPLAY_SINGLE_BUFFER_MODE
    // Restore RED RAM with BW data for correct differential refreshes afterwards
    _eink.cleanupGrayscaleBuffers(sdk_fb);
#endif
    _grayscaleActive = true;
    _recordRefresh("gray", millis() - t0);
    X4_LOG(X4M_DISPLAY_REFRESH_OK);
}

void X4Display::fillRectGray(uint16_t x, uint16_t y, uint16_t w, uint16_t h, GrayLevel shade) {
    if (w == 0 || h == 0 || !_grayPlane) return;
    uint8_t  v      = (uint8_t)shade;
    bool     bwBit  = (v >> 1) & 1u;
    bool     redBit = v & 1u;
    uint8_t* bwFb   = _eink.getFrameBuffer();

    if (_orientation == DisplayOrientation::LANDSCAPE) {
        uint16_t dispW  = _physW();
        uint16_t dispH  = _physH();
        uint16_t wBytes = dispW / 8u;
        uint16_t xEnd   = (x + w < dispW) ? x + w : dispW;
        uint16_t yEnd   = (y + h < dispH) ? (uint16_t)(y + h) : dispH;
        uint8_t  bwFill  = bwBit  ? 0xFF : 0x00;
        uint8_t  redFill = redBit ? 0xFF : 0x00;

        for (uint16_t py = y; py < yEnd; py++) {
            uint16_t px = x;
            // Leading unaligned pixels
            while (px < xEnd && (px % 8) != 0) {
                uint16_t byteIdx = py * wBytes + px / 8u;
                uint8_t  bitMask = 0x80u >> (px % 8u);
                if (bwBit)  bwFb[byteIdx]      |=  bitMask;
                else        bwFb[byteIdx]      &= ~bitMask;
                if (redBit) _grayPlane[byteIdx] |=  bitMask;
                else        _grayPlane[byteIdx] &= ~bitMask;
                px++;
            }
            // Byte-aligned span
            uint16_t fullEnd = xEnd & ~(uint16_t)7u;
            if (fullEnd > px) {
                uint16_t startByte = py * wBytes + px / 8u;
                uint16_t byteCount = (fullEnd - px) / 8u;
                memset(bwFb       + startByte, bwFill,  byteCount);
                memset(_grayPlane + startByte, redFill, byteCount);
                px = fullEnd;
            }
            // Trailing unaligned pixels
            while (px < xEnd) {
                uint16_t byteIdx = py * wBytes + px / 8u;
                uint8_t  bitMask = 0x80u >> (px % 8u);
                if (bwBit)  bwFb[byteIdx]      |=  bitMask;
                else        bwFb[byteIdx]      &= ~bitMask;
                if (redBit) _grayPlane[byteIdx] |=  bitMask;
                else        _grayPlane[byteIdx] &= ~bitMask;
                px++;
            }
        }
    } else {
        // Portrait: pixel-by-pixel with orientation transform
        for (uint16_t row = 0; row < h; row++) {
            for (uint16_t col = 0; col < w; col++) {
                _setGrayPixel(x + col, y + row, shade);
            }
        }
    }
}

void X4Display::drawCharGray(uint16_t x, uint16_t y, char c,
                              GrayLevel fg, GrayLevel bg, uint8_t scale) {
    if (scale == 0) return;
    const BitmapFont* font = _activeFont;
    if (!font) return;

    uint8_t fc = (uint8_t)c;
    if (fc < font->firstChar || fc >= font->firstChar + font->numChars) fc = (uint8_t)' ';

    int glyphIdx = (int)(fc - font->firstChar);
    const uint8_t* glyphData;
    if (font->columnMajor) {
        glyphData = font->data + glyphIdx * (int)font->glyphW;
    } else {
        glyphData = font->data + glyphIdx * (int)font->glyphH * (int)font->bytesPerRow;
    }

    for (uint8_t col = 0; col < font->advance; col++) {
        for (uint8_t row = 0; row < font->glyphH; row++) {
            bool pixOn;
            if (col < font->glyphW) {
                if (font->columnMajor) {
                    pixOn = (glyphData[col] >> row) & 0x01u;
                } else {
                    uint8_t byteInRow = col >> 3u;
                    uint8_t bitMask   = 0x80u >> (col & 7u);
                    pixOn = (glyphData[(int)row * (int)font->bytesPerRow + (int)byteInRow] & bitMask) != 0;
                }
            } else {
                pixOn = false; // gap column → background shade
            }
            GrayLevel shade = pixOn ? fg : bg;
            for (uint8_t sy = 0; sy < scale; sy++) {
                for (uint8_t sx = 0; sx < scale; sx++) {
                    _setGrayPixel(x + (uint16_t)col * scale + sx,
                                  y + (uint16_t)row * scale + sy,
                                  shade);
                }
            }
        }
    }
}

void X4Display::drawTextGray(uint16_t x, uint16_t y, const char* str,
                              GrayLevel fg, GrayLevel bg, uint8_t scale) {
    if (!str || scale == 0) return;
    const BitmapFont* font = _activeFont;
    if (!font) return;
    uint16_t cx      = x;
    uint16_t charAdv = (uint16_t)(font->advance * scale);
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') break;
        if (cx + charAdv > width()) break;
        drawCharGray(cx, y, str[i], fg, bg, scale);
        cx += charAdv;
    }
}

uint16_t X4Display::drawTextWrappedGray(uint16_t x, uint16_t startY, uint16_t maxW,
                                         const char* str,
                                         GrayLevel fg, GrayLevel bg, uint8_t scale) {
    if (!str || scale == 0 || maxW == 0) return startY;
    const BitmapFont* font = _activeFont;
    if (!font) return startY;
    uint16_t charAdv = (uint16_t)(font->advance * scale);
    uint16_t lineH   = (uint16_t)(font->lineH   * scale);
    uint16_t cx = x;
    uint16_t cy = startY;
    const char* p = str;

    while (*p) {
        if (*p == '\n') {
            cx = x;
            cy += lineH;
            p++;
            continue;
        }
        const char* wordEnd = p;
        while (*wordEnd && *wordEnd != ' ' && *wordEnd != '\n') wordEnd++;
        int wordLen = (int)(wordEnd - p);
        uint16_t wordW = (uint16_t)(wordLen * charAdv);

        if (cx + wordW > x + maxW && cx > x) {
            cx = x;
            cy += lineH;
        }
        for (int i = 0; i < wordLen; i++) {
            if (cx + charAdv > x + maxW) {
                cx = x;
                cy += lineH;
            }
            drawCharGray(cx, cy, p[i], fg, bg, scale);
            cx += charAdv;
        }
        p = wordEnd;
        if (*p == ' ') {
            cx += charAdv;
            p++;
        }
    }
    return cy + lineH;
}
