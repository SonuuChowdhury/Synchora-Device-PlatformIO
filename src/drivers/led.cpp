#include <Arduino.h>
#include "led.h"

Led::Led(int pin) {
    _pin = pin;
    pinMode(_pin, OUTPUT);
}

void Led::on() { digitalWrite(_pin, HIGH); }
void Led::off() { digitalWrite(_pin, LOW); }