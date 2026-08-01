#pragma once
#include <Arduino.h>
#include <driver/i2s.h>

class Speaker {
public:
    void init();
    void startTask();
    void write(const uint8_t* buffer, size_t size);
    void playTone(uint32_t frequencyHz, uint32_t durationMs);
    void playChime();
    void playSiren();
    void playEmergencyMelody();
    void playEmergencyAlarm();
    void playNotReadyBeep();
    void stop();
    bool isBusy();
};
