#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// X4Display.h — thin wrapper around EInkDisplay (community-sdk)
//
// Owns the EInkDisplay instance with the correct Xteink X4 hardware pins.
// Emits X4_LOG / X4_DLOG markers, tracks refresh statistics for diagnostics,
// and provides test pattern rendering for OTA health checks.
// ─────────────────────────────────────────────────────────────────────────────

#include <EInkDisplay.h>
#include "../config.h"
#include "../diagnostics/X4Log.h"
#include "../diagnostics/X4Diagnostics.h"

// ── Font constants (used by UI screens that include X4Display.h) ─────────────
static constexpr uint8_t FONT5X7_GLYPH_W = 5;   // pixel columns per glyph
static constexpr uint8_t FONT5X7_GLYPH_H = 7;   // pixel rows per glyph
static constexpr uint8_t FONT5X7_ADVANCE  = 6;   // pixels per character (glyph + gap)
static constexpr uint8_t FONT5X7_LINE_H   = 9;   // line height (glyph + leading)

class X4Display {
public:
    X4Display();

    // Initialize the display hardware. Returns true on success.
    bool init();

    // Frame-level operations
    void fullRefresh();
    // Half-refresh (HALF_REFRESH mode, ~1720 ms): balanced quality and speed.
    // Use for major content transitions where fast refresh ghosting is visible
    // but a full 2-3 s refresh would be jarring.
    void halfRefresh();
    void fastRefresh();
    void partialRefresh(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

    // Clear the screen (fill white) and do a full refresh.
    void clear();

    // Power management
    void sleep();
    void wake();

    // Direct framebuffer access (delegate to EInkDisplay)
    uint8_t* getFrameBuffer();
    uint16_t width()  const;
    uint16_t height() const;

    // Test patterns — used by OTA health check and /api/dev/display/test-pattern
    void renderAllWhite();
    void renderAllBlack();
    void renderCheckerboard();
    void renderBorderWithCornerLabels();
    void renderDiagonalLine();
    void renderFontSample();
    void renderPartialRefreshRect();
    // Select a pattern by name string
    bool renderTestPattern(const char* name);

    // ── Font rendering ───────────────────────────────────────────────────────
    // Draw a single character. `scale` multiplies each pixel (1 = native 5×7).
    // `inverted` swaps black/white (for selection highlights).
    void drawChar(uint8_t* fb, uint16_t x, uint16_t y, char c,
                  bool inverted = false, uint8_t scale = 1);

    // Draw a string (single line, no wrapping). Clips at display edge.
    void drawText(uint8_t* fb, uint16_t x, uint16_t y, const char* str,
                  bool inverted = false, uint8_t scale = 1);

    // Draw a string with word-wrapping within `maxW` pixels from `x`.
    // Returns the y coordinate immediately after the last drawn line.
    uint16_t drawTextWrapped(uint8_t* fb, uint16_t x, uint16_t startY,
                             uint16_t maxW, const char* str,
                             bool inverted = false, uint8_t scale = 1);

    // Fill a rectangle. `black` = fill with black (0), false = white (1).
    void fillRect(uint8_t* fb, uint16_t x, uint16_t y,
                  uint16_t w, uint16_t h, bool black);

    // Diagnostics snapshot
    const X4DisplayStatus& status() const { return _status; }

private:
    EInkDisplay _eink;
    X4DisplayStatus _status;
    bool _initialized = false;

    // Counts consecutive fast refreshes; auto-upgrades to full at the
    // GHOSTING_FULL_REFRESH_INTERVAL threshold to prevent ghosting buildup.
    uint8_t _fastRefreshCount = 0;

    void _recordRefresh(const char* type, uint32_t durationMs);
    void _setError(const char* msg);
};
