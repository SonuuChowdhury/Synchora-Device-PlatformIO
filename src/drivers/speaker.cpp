#include "speaker.h"
#include "../config/pins.h"
#include <driver/i2s.h>
#include <math.h>

#define SPEAKER_I2S_PORT   I2S_NUM_1
#define SAMPLE_RATE        16000
#define DMA_BUF_COUNT      16
#define DMA_BUF_LEN        1024

void Speaker::init() {
    i2s_config_t config = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = DMA_BUF_COUNT,
        .dma_buf_len          = DMA_BUF_LEN,
        .use_apll             = false,
        .tx_desc_auto_clear   = true,
        .fixed_mclk           = 0
    };

    i2s_pin_config_t pins = {
        .bck_io_num   = SPEAKER_BCLK_PIN,
        .ws_io_num    = SPEAKER_LRC_PIN,
        .data_out_num = SPEAKER_DIN_PIN,
        .data_in_num  = I2S_PIN_NO_CHANGE
    };

    esp_err_t err;

    err = i2s_driver_install(SPEAKER_I2S_PORT, &config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[Speaker] ❌ i2s_driver_install failed: %d\n", err);
        return;
    }

    err = i2s_set_pin(SPEAKER_I2S_PORT, &pins);
    if (err != ESP_OK) {
        Serial.printf("[Speaker] ❌ i2s_set_pin failed: %d\n", err);
        return;
    }

    err = i2s_set_clk(SPEAKER_I2S_PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
    if (err != ESP_OK) {
        Serial.printf("[Speaker] ❌ i2s_set_clk failed: %d\n", err);
        return;
    }

    i2s_zero_dma_buffer(SPEAKER_I2S_PORT);

    Serial.println("[Speaker] ✅ MAX98357A I2S Amplifier initialized (LRC:25, BCLK:27, DIN:33)");
}

void Speaker::write(const uint8_t* buffer, size_t size) {
    size_t bytesWritten = 0;
    i2s_write(SPEAKER_I2S_PORT, buffer, size, &bytesWritten, pdMS_TO_TICKS(100));
}

// ============================================================================
// TONE GENERATION
// Note for 16Ω 0.25W Speakers: Recommended frequency range is 500 Hz to 2000 Hz.
// Below 300 Hz is physically inaudible; above 3500 Hz sounds harsh/tinny.
// ============================================================================

void Speaker::playTone(uint32_t frequencyHz, uint32_t durationMs) {
    size_t numFrames = (SAMPLE_RATE * durationMs) / 1000;
    if (numFrames == 0) return;

    // Stereo buffer: 2 int16_t samples per frame (Left and Right)
    int16_t* buffer = (int16_t*)malloc(numFrames * 2 * sizeof(int16_t));
    if (!buffer) return;

    float amplitude = 18000.0f; // Clean peak amplitude without clipping micro-speakers
    size_t fadeSamples = (SAMPLE_RATE * 8) / 1000; // 8 ms smooth fade-in and fade-out
    if (fadeSamples > numFrames / 2) {
        fadeSamples = numFrames / 2;
    }

    for (size_t i = 0; i < numFrames; i++) {
        float env = 1.0f;
        if (fadeSamples > 0) {
            if (i < fadeSamples) {
                env = 0.5f * (1.0f - cosf(M_PI * (float)i / (float)fadeSamples));
            } else if (i >= numFrames - fadeSamples) {
                size_t endIdx = numFrames - 1 - i;
                env = 0.5f * (1.0f - cosf(M_PI * (float)endIdx / (float)fadeSamples));
            }
        }

        float t = (float)i / (float)SAMPLE_RATE;
        int16_t sampleVal = (int16_t)(amplitude * env * sinf(2.0f * M_PI * (float)frequencyHz * t));

        buffer[2 * i]     = sampleVal; // Left channel
        buffer[2 * i + 1] = sampleVal; // Right channel
    }

    write((const uint8_t*)buffer, numFrames * 2 * sizeof(int16_t));
    free(buffer);
}

void Speaker::playChime() {
    Serial.println("[Speaker] 🔔 Playing startup/connection chime...");
    playTone(523, 100); // C5
    delay(15);
    playTone(659, 100); // E5
    delay(15);
    playTone(784, 180); // G5
}

void Speaker::playSiren() {
    Serial.println("[Speaker] 🚨 Playing emergency siren tone...");
    for (int i = 0; i < 3; i++) {
        playTone(880, 100);
        delay(10);
        playTone(600, 100);
        delay(10);
    }
}

void Speaker::playEmergencyMelody() {
    playTone(523,  120); // C5
    delay(10);
    playTone(659,  120); // E5
    delay(10);
    playTone(784,  120); // G5
    delay(10);
    playTone(1047, 260); // C6
    delay(10);
    playTone(784,  120); // G5
    delay(10);
    playTone(659,  120); // E5
}

void Speaker::playEmergencyAlarm() {
    // Balanced 900 Hz / 700 Hz alternating emergency alarm warble
    playTone(900, 90);
    delay(10);
    playTone(700, 90);
    delay(10);
    playTone(900, 90);
    delay(10);
    playTone(700, 90);
}

void Speaker::playNotReadyBeep() {
    // Modern dual-note rising status chirp (C5 523 Hz -> G5 784 Hz) with smooth envelope
    playTone(523, 40);
    delay(10);
    playTone(784, 50);
}

void Speaker::stop() {
    i2s_zero_dma_buffer(SPEAKER_I2S_PORT);
}
