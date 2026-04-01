#pragma once

class Button {
public:
    Button(int pin);
    bool isPressed();
private:
    int _pin;
};