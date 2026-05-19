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

class X4Display {
public:
    X4Display();

    // Initialize the display hardware. Returns true on success.
    bool init();

    // Frame-level operations
    void fullRefresh();
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

    // Diagnostics snapshot
    const X4DisplayStatus& status() const { return _status; }

private:
    EInkDisplay _eink;
    X4DisplayStatus _status;
    bool _initialized = false;

    void _recordRefresh(const char* type, uint32_t durationMs);
    void _setError(const char* msg);
};
