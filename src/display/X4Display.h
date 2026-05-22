#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// X4Display.h — thin wrapper around EInkDisplay (community-sdk)
//
// Owns the EInkDisplay instance with the correct Xteink X4 hardware pins.
// Emits X4_LOG / X4_DLOG markers, tracks refresh statistics for diagnostics,
// and provides test pattern rendering for OTA health checks.
//
// Orientation
// ───────────
// The physical panel is always 800 × 480 px (landscape).  setOrientation()
// installs a software coordinate transform so that the drawing primitives
// (drawChar, fillRect, drawText, drawTextWrapped) operate in a *logical*
// coordinate space whose origin and axes match the chosen orientation:
//
//   LANDSCAPE      – logical (0,0) = physical top-left.   800 × 480.
//   PORTRAIT_CW    – device rotated 90° CW (right edge up).
//                    logical (0,0) = physical top-right.  480 × 800.
//   PORTRAIT_CCW   – device rotated 90° CCW (left edge up).
//                    logical (0,0) = physical bottom-left. 480 × 800.
//
// width() / height() always return the *logical* dimensions so that layout
// code is orientation-agnostic.  The physical framebuffer is still 48 KB
// regardless of orientation, so memset() clears are always 48 000 bytes.
//
// Fonts
// ─────
// setFont() installs a BitmapFont for subsequent draw calls.  Pass nullptr
// to revert to the built-in 5×7 font.  charAdvance(scale) / lineHeight(scale)
// / glyphHeight(scale) return scaled metrics of the active font so that UI
// screens remain orientation- and font-agnostic.
//
// 4-Level Grayscale
// ─────────────────
// The SSD1677 drives the GDEQ0426T82 (B/W only) panel with a custom LUT that
// maps each of the four (BW-bit, RED-bit) RAM combinations to a distinct waveform,
// producing four gray levels without any physical red ink:
//
//   GrayLevel  BW-bit  RED-bit  Appearance
//   BLACK        0       0      Full black
//   DARK_GRAY    0       1      Dark gray
//   LIGHT_GRAY   1       0      Light gray
//   WHITE        1       1      Full white
//
// The BW plane lives in the SDK framebuffer (CMD 0x24).  The RED plane is the
// 48 KB `_grayPlane` member (CMD 0x26).  Gray drawing primitives write both
// planes simultaneously.  Call clearFrameGrayscale() to reset both planes to
// WHITE, then use the *Gray() variants of all draw methods, and finish with
// displayGrayscale() to push both planes to hardware and trigger the refresh.
//
// A frame must use EITHER the 1-bit API (fillRect / drawChar / …) OR the
// grayscale API (fillRectGray / drawCharGray / …) — mixing them within a
// single frame produces undefined gray levels because the 1-bit API leaves
// _grayPlane untouched.
// ─────────────────────────────────────────────────────────────────────────────

#include <EInkDisplay.h>
#include "BitmapFont.h"
#include "../config.h"
#include "../diagnostics/X4Log.h"
#include "../diagnostics/X4Diagnostics.h"

// ── Backward-compatible Font5x7 constants ────────────────────────────────────
// Kept so legacy callers that hard-code Font5x7 metrics still compile.
static constexpr uint8_t FONT5X7_GLYPH_W = 5;   ///< pixel columns per glyph
static constexpr uint8_t FONT5X7_GLYPH_H = 7;   ///< pixel rows per glyph
static constexpr uint8_t FONT5X7_ADVANCE  = 6;   ///< pixels per character (glyph + gap)
static constexpr uint8_t FONT5X7_LINE_H   = 9;   ///< line height (glyph + leading)

// ── Orientation ──────────────────────────────────────────────────────────────
enum class DisplayOrientation : uint8_t {
    LANDSCAPE     = 0,  ///< Physical panel in native landscape orientation (default)
    PORTRAIT_CW   = 1,  ///< Device rotated 90° CW  – right edge up, logical 480×800
    PORTRAIT_CCW  = 2,  ///< Device rotated 90° CCW – left edge up,  logical 480×800
};

// ── 4-level gray shade ────────────────────────────────────────────────────────
// Values encode (BW-bit<<1 | RED-bit), matching the SSD1677 LUT index directly.
enum class GrayLevel : uint8_t {
    BLACK      = 0,  ///< BW=0, RED=0 — full black
    DARK_GRAY  = 1,  ///< BW=0, RED=1 — dark gray
    LIGHT_GRAY = 2,  ///< BW=1, RED=0 — light gray
    WHITE      = 3,  ///< BW=1, RED=1 — full white
};

class X4Display {
public:
    X4Display();

    // Initialize the display hardware. Returns true on success.
    bool init();

    // ── Frame-level operations ───────────────────────────────────────────────
    void fullRefresh();
    // Half-refresh (~1720 ms): balanced quality and speed.
    void halfRefresh();
    void fastRefresh();
    // Partial refresh of a physical rectangle (physical coordinates).
    void partialRefresh(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

    // Clear the screen (fill white) and do a full refresh.
    void clear();

    // ── Grayscale frame operations ───────────────────────────────────────────
    /// Reset both the BW plane and the RED plane to WHITE.
    /// Call at the start of every grayscale frame instead of memset(fb, 0xFF).
    void clearFrameGrayscale();

    /// Push both planes to hardware and trigger a grayscale (4-level) refresh.
    /// Uses the SDK custom LUT sequence; always a full waveform cycle.
    void displayGrayscale();

    /// Returns true if the most recent refresh was a grayscale refresh.
    /// Used by the screenshot endpoint to choose between 1bpp and 2bpp BMP.
    bool isGrayscale() const { return _grayscaleActive; }

    /// Raw access to the RED plane for screenshot export.
    const uint8_t* getGrayPlane() const { return _grayPlane; }  // may be nullptr if init() failed

    // ── Power management ─────────────────────────────────────────────────────
    void sleep();
    void wake();

    // ── Direct framebuffer / dimension access ─────────────────────────────────
    uint8_t* getFrameBuffer();
    /// Logical width (depends on orientation: 800 for landscape, 480 for portrait).
    uint16_t width()  const;
    /// Logical height (depends on orientation: 480 for landscape, 800 for portrait).
    uint16_t height() const;

    // ── Orientation ──────────────────────────────────────────────────────────
    void               setOrientation(DisplayOrientation o);
    DisplayOrientation getOrientation() const { return _orientation; }

    // ── Font selection ────────────────────────────────────────────────────────
    /// Install a font for subsequent draw calls.  Pass nullptr for Font5x7.
    void               setFont(const BitmapFont* font);
    const BitmapFont*  activeFont() const { return _activeFont; }

    // Scaled metrics of the active font.
    uint16_t charAdvance (uint8_t scale = 1) const;
    uint16_t lineHeight  (uint8_t scale = 1) const;
    uint16_t glyphHeight (uint8_t scale = 1) const;

    // ── Test patterns ─────────────────────────────────────────────────────────
    void renderAllWhite();
    void renderAllBlack();
    void renderCheckerboard();
    void renderBorderWithCornerLabels();
    void renderDiagonalLine();
    void renderFontSample();
    void renderPartialRefreshRect();
    bool renderTestPattern(const char* name);

    // ── Font rendering (logical coordinates, active font, active orientation) ──
    /// Draw a single character. `scale` multiplies each pixel (1 = native size).
    /// `inverted` swaps black/white (for selection highlights).
    void drawChar(uint8_t* fb, uint16_t x, uint16_t y, char c,
                  bool inverted = false, uint8_t scale = 1);

    /// Draw a string (single line, no wrapping).  Clips at logical display edge.
    void drawText(uint8_t* fb, uint16_t x, uint16_t y, const char* str,
                  bool inverted = false, uint8_t scale = 1);

    /// Draw a string with word-wrapping within `maxW` logical pixels from `x`.
    /// Returns the logical y coordinate immediately after the last drawn line.
    uint16_t drawTextWrapped(uint8_t* fb, uint16_t x, uint16_t startY,
                             uint16_t maxW, const char* str,
                             bool inverted = false, uint8_t scale = 1);

    /// Fill a rectangle in logical coordinates. `black` = fill with black.
    void fillRect(uint8_t* fb, uint16_t x, uint16_t y,
                  uint16_t w, uint16_t h, bool black);

    // ── Grayscale font/fill rendering (writes BW + RED planes) ───────────────
    /// Fill a rectangle with the given gray shade. Writes both planes.
    void fillRectGray(uint16_t x, uint16_t y, uint16_t w, uint16_t h, GrayLevel shade);

    /// Draw a character with independent foreground and background shade.
    void drawCharGray(uint16_t x, uint16_t y, char c,
                      GrayLevel fg, GrayLevel bg, uint8_t scale = 1);

    /// Draw a string (single line) with foreground/background shade.
    void drawTextGray(uint16_t x, uint16_t y, const char* str,
                      GrayLevel fg, GrayLevel bg, uint8_t scale = 1);

    /// Draw a word-wrapped string with foreground/background shade.
    /// Returns the logical y coordinate immediately after the last drawn line.
    uint16_t drawTextWrappedGray(uint16_t x, uint16_t startY, uint16_t maxW,
                                 const char* str,
                                 GrayLevel fg, GrayLevel bg, uint8_t scale = 1);

    // ── Diagnostics ──────────────────────────────────────────────────────────
    const X4DisplayStatus& status() const { return _status; }

private:
    EInkDisplay        _eink;
    X4DisplayStatus    _status;
    bool               _initialized    = false;
    bool               _grayscaleActive = false;  ///< true after displayGrayscale(), cleared by 1-bit refreshes
    uint8_t            _fastRefreshCount = 0;
    DisplayOrientation _orientation    = DisplayOrientation::LANDSCAPE;
    const BitmapFont*  _activeFont     = nullptr;  // nullptr → FONT_5X7 (set in init)

    // Second 1-bit plane (RED RAM, CMD 0x26).  Combined with the SDK BW plane
    // this encodes all four GrayLevel shades.  Size matches the physical buffer.
    // Heap-allocated in init() to avoid overflowing the static DRAM segment.
    uint8_t* _grayPlane = nullptr;

    // Physical panel dimensions (constant, orientation-independent).
    uint16_t _physW() const { return _eink.getDisplayWidth();  }
    uint16_t _physH() const { return _eink.getDisplayHeight(); }

    // Write a single pixel at logical coordinates (lx, ly) applying the
    // current orientation transform.  Out-of-bounds writes are silently dropped.
    void _setPixel(uint8_t* fb, uint16_t lx, uint16_t ly, bool black);

    void _recordRefresh(const char* type, uint32_t durationMs);
    void _setError(const char* msg);

    // Write a single logical pixel to both the BW plane and the RED plane,
    // applying the current orientation transform.
    void _setGrayPixel(uint16_t lx, uint16_t ly, GrayLevel level);
};
