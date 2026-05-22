#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// SleepScreen.h — "Going to sleep" screen for the e-paper display
//
// Supports four display modes, configurable via NVS (namespace "sleep"):
//
//   CLOCK       (mode=0)  — date/time + battery. Default.
//   BMP_FILE    (mode=1)  — single 1-bit BMP from SD card path "img_path".
//   BMP_FOLDER  (mode=2)  — cycles through 1-bit BMP files in SD folder
//                           "img_dir" (default "/sleep"), advancing the index
//                           stored in NVS key "img_idx" on each sleep.
//   THEME_COVER (mode=3)  — journal theme-system dashboard (portrait 480×800).
//                           Requires a ThemeScreen to be registered via
//                           setThemeScreen().
//
// All modes fall back to CLOCK if required resources are unavailable.
//
// Call loadConfig() once after storage is initialised to populate the settings
// from NVS. Call setThemeScreen() before the first sleep() call if you want
// THEME_COVER mode to be available.
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include "../display/X4Display.h"
#include "../system/X4Clock.h"

class ThemeScreen;

// ── Sleep mode ────────────────────────────────────────────────────────────────
enum class SleepMode : uint8_t {
    CLOCK       = 0,   ///< Date/time clock display (default)
    BMP_FILE    = 1,   ///< Single 1-bit BMP from a configurable SD path
    BMP_FOLDER  = 2,   ///< Slideshow — cycle through BMPs in a folder
    THEME_COVER = 3,   ///< Journal theme-system cover page (portrait)
};

class SleepScreen {
public:
    SleepScreen(X4Display& display, X4Clock& clock);

    // Register the ThemeScreen used by THEME_COVER mode.
    void setThemeScreen(ThemeScreen* ts);

    // Read NVS namespace "sleep" and populate _mode / _imagePath / _imageDir.
    // Call once after storage is initialised.
    void loadConfig();

    // Render the sleep screen, put the EPD to sleep, and enter deep sleep.
    // Does not return.
    [[noreturn]] void sleep(uint8_t batteryPct = 0);

private:
    X4Display&   _display;
    X4Clock&     _clock;
    ThemeScreen* _themeScreen = nullptr;

    SleepMode _mode     = SleepMode::CLOCK;
    String    _imagePath;                     // BMP_FILE path
    String    _imageDir = "/sleep";           // BMP_FOLDER directory

    // ── Render helpers ────────────────────────────────────────────────────────
    void _renderClock(uint8_t batteryPct);
    bool _renderBmpFile(const char* path);
    bool _renderBmpFolder();
    void _renderThemeCover();

    // Read one 1-bit BMP from SD and blit to the grayscale framebuffer.
    // Returns true on success.
    bool _blitBmp(const char* path);

    static uint32_t _readU32LE(const uint8_t* p);
    static  int32_t _readS32LE(const uint8_t* p);
    static uint16_t _readU16LE(const uint8_t* p);
};
