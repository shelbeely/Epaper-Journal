#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// X4Input.h — thin wrapper around InputManager (community-sdk)
//
// Owns the InputManager instance.  Provides named helpers for each button,
// and a safe-mode GPIO pre-read before InputManager::begin().
// ─────────────────────────────────────────────────────────────────────────────

#include <InputManager.h>
#include "../config.h"
#include "../diagnostics/X4Log.h"

class X4Input {
public:
    X4Input();

    // Must be called once in setup(), AFTER the safe-mode GPIO check.
    bool init();

    // Call every loop() iteration — drives debounce + event detection.
    void tick();

    // ── Named button helpers (single-press events) ──────────────────────
    bool wasLeft()    const;
    bool wasRight()   const;
    bool wasUp()      const;
    bool wasDown()    const;
    bool wasConfirm() const;
    bool wasBack()    const;

    // True if Confirm was released after being held for >= ms milliseconds.
    bool wasConfirmHeld(uint32_t ms) const;

    // Power button current state
    bool isPowerButtonPressed() const;

    // Any button was pressed this tick
    bool wasAnyPressed() const;

    // Access the underlying InputManager for advanced use
    InputManager& raw() { return _im; }

    // ── Safe-mode pre-read ───────────────────────────────────────────────
    // Read the power-button GPIO directly (no InputManager needed).
    // Call this before init() in setup() to detect boot-time safe mode.
    static bool readPowerButtonGpio();

private:
    InputManager _im;
    bool _initialized = false;
};
