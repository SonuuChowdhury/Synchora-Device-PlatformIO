#pragma once
#include <Arduino.h>
#include <driver/i2s.h>

class Mic {
public:
    void init();
    void test();
    int  read(uint8_t* buffer, size_t numSamples);
};