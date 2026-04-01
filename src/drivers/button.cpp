#include <Arduino.h>
#include "button.h"

Button::Button(int pin) {
    _pin = pin;
    pinMode(_pin, INPUT_PULLUP);
}

bool Button::isPressed() {
    return digitalRead(_pin) == LOW;
}