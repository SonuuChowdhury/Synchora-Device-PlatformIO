#pragma once

class Led {
public:
    Led(int pin);
    void on();
    void off();
private:
    int _pin;
};