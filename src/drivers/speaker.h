#pragma once
#include <Arduino.h>
#include <driver/i2s.h>

class Speaker {
public:
    void init();
    void write(const uint8_t* buffer, size_t size);
    // Frequency Limit Note (16Ω 0.25W Micro-Speakers):
    // Keep tone frequency between 500 Hz and 2000 Hz for maximum loudness, clarity, and safety.
    // Tones below 300 Hz are physically silent; tones above 3500 Hz sound harsh/tinny.
    void playTone(uint32_t frequencyHz, uint32_t durationMs);
    void playChime();
    void playSiren();
    void playEmergencyMelody();
    void playEmergencyAlarm();
    void playNotReadyBeep();
    void stop();
};
