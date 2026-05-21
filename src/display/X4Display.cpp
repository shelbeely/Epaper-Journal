// ─────────────────────────────────────────────────────────────────────────────
// X4Display.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "X4Display.h"
#include "Font5x7.h"
#include <Arduino.h>
#include <string.h>

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

    X4_DLOG(X4M_DISPLAY_FRAMEBUFFER_ALLOC);
    X4_LOG(X4M_DISPLAY_INIT_OK);
    return true;
}

// ── Refresh helpers ──────────────────────────────────────────────────────────

void X4Display::fullRefresh() {
    X4_DLOG(X4M_DISPLAY_FULL_REFRESH_START);
    uint32_t t0 = millis();
    _fastRefreshCount = 0;
    _eink.displayBuffer(EInkDisplay::FULL_REFRESH);
    _recordRefresh("full", millis() - t0);
    X4_LOG(X4M_DISPLAY_REFRESH_OK);
}

void X4Display::halfRefresh() {
    uint32_t t0 = millis();
    _fastRefreshCount = 0;
    _eink.displayBuffer(EInkDisplay::HALF_REFRESH);
    _recordRefresh("half", millis() - t0);
    X4_LOG(X4M_DISPLAY_REFRESH_OK);
}

void X4Display::fastRefresh() {
    uint32_t t0 = millis();

    // Auto-upgrade to a full refresh after GHOSTING_FULL_REFRESH_INTERVAL
    // consecutive fast refreshes to clear accumulated ghosting artifacts.
#if GHOSTING_FULL_REFRESH_INTERVAL > 0
    if (++_fastRefreshCount >= GHOSTING_FULL_REFRESH_INTERVAL) {
        _fastRefreshCount = 0;
        _eink.displayBuffer(EInkDisplay::FULL_REFRESH);
        _recordRefresh("full-ghost", millis() - t0);
    } else {
        _eink.displayBuffer(EInkDisplay::FAST_REFRESH);
        _recordRefresh("fast", millis() - t0);
    }
#else
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
    _eink.clearScreen(0xFF); // 0xFF = all white
    fullRefresh();
}

// ── Power management ─────────────────────────────────────────────────────────

void X4Display::sleep() {
    _eink.deepSleep();
    X4_DLOG(X4M_DISPLAY_SLEEP_OK);
}

void X4Display::wake() {
    // deepSleep() requires a hardware reset to wake; re-init the controller.
    _fastRefreshCount = 0;
    _eink.begin();
    X4_DLOG(X4M_DISPLAY_WAKE_OK);
}

// ── Framebuffer access ───────────────────────────────────────────────────────

uint8_t* X4Display::getFrameBuffer() {
    return _eink.getFrameBuffer();
}

uint16_t X4Display::width() const {
    return _eink.getDisplayWidth();
}

uint16_t X4Display::height() const {
    return _eink.getDisplayHeight();
}

// ── Test patterns ────────────────────────────────────────────────────────────

void X4Display::renderAllWhite() {
    _eink.clearScreen(0xFF);
    fullRefresh();
}

void X4Display::renderAllBlack() {
    _eink.clearScreen(0x00);
    fullRefresh();
}

void X4Display::renderCheckerboard() {
    uint8_t* fb = _eink.getFrameBuffer();
    uint16_t wBytes = _eink.getDisplayWidthBytes();
    uint16_t h      = _eink.getDisplayHeight();

    for (uint16_t row = 0; row < h; row++) {
        for (uint16_t col = 0; col < wBytes; col++) {
            // alternate 0xAA / 0x55 per byte, invert on odd rows
            uint8_t pattern = (col % 2 == 0) ? 0xAA : 0x55;
            if (row % 2 != 0) pattern = ~pattern;
            fb[row * wBytes + col] = pattern;
        }
    }
    fullRefresh();
}

void X4Display::renderBorderWithCornerLabels() {
    _eink.clearScreen(0xFF);
    uint8_t* fb    = _eink.getFrameBuffer();
    uint16_t wBytes = _eink.getDisplayWidthBytes();
    uint16_t w      = _eink.getDisplayWidth();
    uint16_t h      = _eink.getDisplayHeight();

    // Draw top and bottom borders (1px black line)
    for (uint16_t col = 0; col < wBytes; col++) {
        fb[col] = 0x00;                         // top row
        fb[(h - 1) * wBytes + col] = 0x00;     // bottom row
    }
    // Draw left and right borders
    for (uint16_t row = 0; row < h; row++) {
        // Left border: set MSB of first byte
        fb[row * wBytes] &= 0x7F;
        // Right border: set LSB of last byte
        fb[row * wBytes + wBytes - 1] &= 0xFE;
    }
    // Small marker squares at corners (4×4 pixels, black)
    auto markCorner = [&](uint16_t px, uint16_t py) {
        for (uint16_t dy = 0; dy < 4 && (py + dy) < h; dy++) {
            for (uint16_t dx = 0; dx < 4 && (px + dx) < w; dx++) {
                uint16_t byteIdx = (py + dy) * wBytes + (px + dx) / 8;
                uint8_t  bitMask = 0x80 >> ((px + dx) % 8);
                fb[byteIdx] &= ~bitMask; // set bit to 0 = black
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
    uint8_t* fb    = _eink.getFrameBuffer();
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
    // Without a GFX library, render a simple repeated bit pattern that
    // looks like a row of squares — a placeholder until a font renderer is added.
    _eink.clearScreen(0xFF);
    uint8_t* fb     = _eink.getFrameBuffer();
    uint16_t wBytes = _eink.getDisplayWidthBytes();
    uint16_t h      = _eink.getDisplayHeight();

    // Draw solid 8×8 pixel blocks at fixed positions (simulates text glyphs)
    auto drawBlock = [&](uint16_t bx, uint16_t by) {
        for (uint16_t row = by; row < by + 8 && row < h; row++) {
            fb[row * wBytes + bx] = 0x00; // 8 black pixels
        }
    };
    const char* label = "JOURNAL"; // 7 chars
    for (int i = 0; i < 7; i++) {
        drawBlock(10 + i * 10, 20);
    }
    fullRefresh();
}

void X4Display::renderPartialRefreshRect() {
    // Render a black rectangle in the center using displayWindow()
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
    if (strcmp(name, "all_white")              == 0) { renderAllWhite();              return true; }
    if (strcmp(name, "all_black")              == 0) { renderAllBlack();              return true; }
    if (strcmp(name, "checkerboard")           == 0) { renderCheckerboard();          return true; }
    if (strcmp(name, "border")                 == 0) { renderBorderWithCornerLabels(); return true; }
    if (strcmp(name, "diagonal")               == 0) { renderDiagonalLine();          return true; }
    if (strcmp(name, "font_sample")            == 0) { renderFontSample();            return true; }
    if (strcmp(name, "partial_refresh_rect")   == 0) { renderPartialRefreshRect();    return true; }
    return false;
}

// ── Private helpers ──────────────────────────────────────────────────────────

void X4Display::_recordRefresh(const char* type, uint32_t durationMs) {
    _status.lastRefreshType = type;
    _status.lastRefreshMs   = durationMs;
}

void X4Display::_setError(const char* msg) {
    _status.lastError = msg;
    X4_LOG(X4M_DISPLAY_INIT_FAILED);
}

// ── Font rendering ────────────────────────────────────────────────────────────

void X4Display::drawChar(uint8_t* fb, uint16_t x, uint16_t y, char c,
                          bool inverted, uint8_t scale) {
    if (!fb || scale == 0) return;
    if (c < 0x20 || c > 0x7E) c = ' ';
    const uint8_t* glyph = FONT5X7[c - 0x20];

    uint16_t dispW  = width();
    uint16_t dispH  = height();
    uint16_t wBytes = dispW / 8;

    // Draw FONT5X7_ADVANCE columns (5 glyph + 1 gap)
    for (uint8_t col = 0; col < FONT5X7_ADVANCE; col++) {
        uint8_t colData = (col < FONT5X7_GLYPH_W) ? glyph[col] : 0x00;
        for (uint8_t row = 0; row < FONT5X7_GLYPH_H; row++) {
            bool pixOn = (colData >> row) & 0x01; // bit0 = top row
            bool drawBlack = inverted ? !pixOn : pixOn;
            if (!drawBlack && !inverted) continue; // skip white pixels in normal mode
            for (uint8_t sy = 0; sy < scale; sy++) {
                for (uint8_t sx = 0; sx < scale; sx++) {
                    uint16_t px = x + col * scale + sx;
                    uint16_t py = y + row * scale + sy;
                    if (px >= dispW || py >= dispH) continue;
                    uint16_t byteIdx = py * wBytes + px / 8;
                    uint8_t  bitMask = 0x80 >> (px % 8);
                    if (drawBlack) {
                        fb[byteIdx] &= ~bitMask; // black
                    } else {
                        fb[byteIdx] |= bitMask;  // white
                    }
                }
            }
        }
    }
}

void X4Display::drawText(uint8_t* fb, uint16_t x, uint16_t y, const char* str,
                          bool inverted, uint8_t scale) {
    if (!fb || !str || scale == 0) return;
    uint16_t cx = x;
    uint16_t charAdv = (uint16_t)(FONT5X7_ADVANCE * scale);
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') break; // single-line: stop at newline
        if (cx + charAdv > width()) break; // clip at right edge
        drawChar(fb, cx, y, str[i], inverted, scale);
        cx += charAdv;
    }
}

uint16_t X4Display::drawTextWrapped(uint8_t* fb, uint16_t x, uint16_t startY,
                                     uint16_t maxW, const char* str,
                                     bool inverted, uint8_t scale) {
    if (!fb || !str || scale == 0 || maxW == 0) return startY;
    uint16_t charAdv = (uint16_t)(FONT5X7_ADVANCE * scale);
    uint16_t lineH   = (uint16_t)(FONT5X7_LINE_H   * scale);
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
        // Find end of current word
        const char* wordEnd = p;
        while (*wordEnd && *wordEnd != ' ' && *wordEnd != '\n') wordEnd++;
        int wordLen = (int)(wordEnd - p);
        uint16_t wordW = (uint16_t)(wordLen * charAdv);

        // Wrap if word doesn't fit and we're not at the start of the line
        if (cx + wordW > x + maxW && cx > x) {
            cx = x;
            cy += lineH;
        }

        // Draw the word character by character
        for (int i = 0; i < wordLen; i++) {
            if (cx + charAdv > x + maxW) {
                // Hard-break mid-word
                cx = x;
                cy += lineH;
            }
            drawChar(fb, cx, cy, p[i], inverted, scale);
            cx += charAdv;
        }

        p = wordEnd;
        if (*p == ' ') {
            cx += charAdv; // space gap
            p++;
        }
    }
    return cy + lineH;
}

void X4Display::fillRect(uint8_t* fb, uint16_t x, uint16_t y,
                          uint16_t w, uint16_t h, bool black) {
    if (!fb || w == 0 || h == 0) return;
    uint16_t dispW  = width();
    uint16_t dispH  = height();
    uint16_t wBytes = dispW / 8;

    uint16_t xEnd = (x + w < dispW) ? x + w : dispW;
    uint8_t  fill = black ? 0x00 : 0xFF;

    for (uint16_t py = y; py < y + h && py < dispH; py++) {
        uint16_t px = x;

        // ── Leading unaligned pixels (until the next byte boundary) ──────────
        while (px < xEnd && (px % 8) != 0) {
            uint16_t byteIdx = py * wBytes + px / 8;
            uint8_t  bitMask = 0x80 >> (px % 8);
            if (black) fb[byteIdx] &= ~bitMask;
            else       fb[byteIdx] |=  bitMask;
            px++;
        }

        // ── Full-byte aligned span: write 8 pixels per byte ──────────────────
        uint16_t fullEnd = xEnd & ~0x7u; // round down to byte boundary
        if (fullEnd > px) {
            uint16_t startByte = px / 8;
            uint16_t byteCount = (fullEnd - px) / 8;
            memset(fb + py * wBytes + startByte, fill, byteCount);
            px = fullEnd;
        }

        // ── Trailing unaligned pixels ─────────────────────────────────────────
        while (px < xEnd) {
            uint16_t byteIdx = py * wBytes + px / 8;
            uint8_t  bitMask = 0x80 >> (px % 8);
            if (black) fb[byteIdx] &= ~bitMask;
            else       fb[byteIdx] |=  bitMask;
            px++;
        }
    }
}
