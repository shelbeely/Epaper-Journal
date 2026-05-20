#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// SleepScreen.h — Minimal "going to sleep" screen for the e-paper display
//
// Shows a brief sleep message (date/time + "ZZZ"), does a full refresh,
// puts the EPD driver to sleep, then calls esp_deep_sleep_start().
// The function never returns.
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include "../display/X4Display.h"
#include "../system/X4Clock.h"

class SleepScreen {
public:
    SleepScreen(X4Display& display, X4Clock& clock);

    // Render the sleep screen, put the EPD to sleep, and enter deep sleep.
    // Does not return.
    [[noreturn]] void sleep(uint8_t batteryPct = 0);

private:
    X4Display& _display;
    X4Clock&   _clock;
};
