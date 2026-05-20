#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// PinScreen.h — On-device 4-digit PIN entry UI for vault unlock/set
//
// Displays four digit slots. Navigation:
//   Up / Down — increment / decrement the current digit (0–9, wraps)
//   Left / Right — move to the previous / next digit
//   Confirm — submit the PIN; the vault derives a key and unlocks
//   Back — cancel without changing the vault state
//
// Returns true if the user submitted a PIN (vault is now unlocked),
// false if the user cancelled.
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include "../vault/VaultManager.h"
#include "../display/X4Display.h"
#include "../input/X4Input.h"

class PinScreen {
public:
    PinScreen(X4Display& display, X4Input& input);

    // Show the PIN entry UI (blocking).
    // On Confirm: derives key from PIN → unlocks vault → returns true.
    // On Back: returns false without modifying the vault.
    bool run(VaultManager& vault);

private:
    X4Display& _display;
    X4Input&   _input;

    static constexpr uint8_t PIN_LEN   = 4;
    static constexpr uint8_t SCALE     = 4;  // large digits
    static constexpr uint8_t BOX_W     = 60; // pixel width of each digit box
    static constexpr uint8_t BOX_H     = 70; // pixel height of each digit box
    static constexpr uint8_t BOX_GAP   = 20; // gap between boxes

    void _render(const uint8_t digits[PIN_LEN], uint8_t cursor);
};
