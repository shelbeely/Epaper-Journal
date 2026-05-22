// ─────────────────────────────────────────────────────────────────────────────
// SleepScreen.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "SleepScreen.h"
#include <esp_sleep.h>
#include <string.h>
#include <stdio.h>

SleepScreen::SleepScreen(X4Display& display, X4Clock& clock)
    : _display(display), _clock(clock)
{}

void SleepScreen::sleep(uint8_t batteryPct) {
    uint8_t* fb    = _display.getFrameBuffer();
    uint16_t dispW = _display.width();
    uint16_t dispH = _display.height();

    // Clear to white
    memset(fb, 0xFF, (size_t)(dispW / 8) * dispH);

    // ── Date / time (centered) ────────────────────────────────────────────────
    struct tm now = _clock.now();
    char timeBuf[9];   // "HH:MM:SS"
    char dateBuf[12];  // "YYYY-MM-DD"
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
             now.tm_hour, now.tm_min, now.tm_sec);
    snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d",
             now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);

    // Scale 3 for time, scale 2 for date
    constexpr uint8_t TSCALE = 3;
    constexpr uint8_t DSCALE = 2;

    uint16_t timeW = (uint16_t)(strlen(timeBuf)  * _display.charAdvance(TSCALE));
    uint16_t dateW = (uint16_t)(strlen(dateBuf) * _display.charAdvance(DSCALE));

    uint16_t timeY = dispH / 2 - _display.lineHeight(TSCALE) - 4;
    uint16_t dateY = dispH / 2 + 4;

    _display.drawText(fb, (dispW - timeW) / 2, timeY, timeBuf, false, TSCALE);
    _display.drawText(fb, (dispW - dateW) / 2, dateY, dateBuf, false, DSCALE);

    // ── "ZZZ" bottom-right ────────────────────────────────────────────────────
    constexpr uint8_t ZSCALE = 2;
    uint16_t zW = (uint16_t)(3 * _display.charAdvance(ZSCALE));
    uint16_t zY = dispH - _display.lineHeight(ZSCALE) - 8;
    _display.drawText(fb, dispW - zW - 8, zY, "ZZZ", false, ZSCALE);

    // ── Battery (top-right corner) ────────────────────────────────────────────
    if (batteryPct > 0) {
        char batBuf[8];
        snprintf(batBuf, sizeof(batBuf), "%u%%", batteryPct);
        uint16_t batW = (uint16_t)(strlen(batBuf) * _display.charAdvance(2));
        _display.drawText(fb, dispW - batW - 4, 4, batBuf, false, 2);
    }

    _display.fullRefresh();
    _display.sleep();
    esp_deep_sleep_start();
    // unreachable — annotated [[noreturn]] in header
    __builtin_unreachable();
}
