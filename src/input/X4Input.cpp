// ─────────────────────────────────────────────────────────────────────────────
// X4Input.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "X4Input.h"
#include <Arduino.h>

X4Input::X4Input() {}

bool X4Input::init() {
    _im.begin();
    _initialized = true;
    X4_LOG(X4M_INPUT_OK);
    return true;
}

void X4Input::tick() {
    if (_initialized) {
        _im.update();
    }
}

bool X4Input::wasLeft()    const { return _im.wasPressed(InputManager::BTN_LEFT);    }
bool X4Input::wasRight()   const { return _im.wasPressed(InputManager::BTN_RIGHT);   }
bool X4Input::wasUp()      const { return _im.wasPressed(InputManager::BTN_UP);      }
bool X4Input::wasDown()    const { return _im.wasPressed(InputManager::BTN_DOWN);    }
bool X4Input::wasConfirm() const { return _im.wasPressed(InputManager::BTN_CONFIRM); }
bool X4Input::wasBack()    const { return _im.wasPressed(InputManager::BTN_BACK);    }

bool X4Input::wasConfirmHeld(uint32_t ms) const {
    return _im.wasReleased(InputManager::BTN_CONFIRM) &&
           _im.getHeldTime() >= ms;
}

bool X4Input::isPowerButtonPressed() const {
    return _im.isPowerButtonPressed();
}

bool X4Input::wasAnyPressed() const {
    return _im.wasAnyPressed();
}

/*static*/
bool X4Input::readPowerButtonGpio() {
    // Read the power button GPIO directly, before InputManager::begin().
    // The pin uses an internal pull-up; button pressed = LOW.
    pinMode(POWER_BUTTON_GPIO, INPUT_PULLUP);
    // Brief settle
    delay(5);
    return (digitalRead(POWER_BUTTON_GPIO) == LOW);
}
