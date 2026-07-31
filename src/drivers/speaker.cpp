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
        .communication_format = I2S_COMM_FORMAT_I2S,
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
    size_t samplesCount = (SAMPLE_RATE * durationMs) / 1000;
    int16_t* buffer = (int16_t*)malloc(samplesCount * sizeof(int16_t));
    if (!buffer) return;

    float amplitude = 30000.0f; // Maximum 16-bit peak volume amplitude
    for (size_t i = 0; i < samplesCount; i++) {
        float t = (float)i / (float)SAMPLE_RATE;
        buffer[i] = (int16_t)(amplitude * sinf(2.0f * M_PI * (float)frequencyHz * t));
    }

    write((const uint8_t*)buffer, samplesCount * sizeof(int16_t));
    free(buffer);
}

void Speaker::playChime() {
    Serial.println("[Speaker] 🔔 Playing startup chime...");
    playTone(523, 120); // C5
    playTone(659, 120); // E5
    playTone(784, 200); // G5
}

void Speaker::playSiren() {
    Serial.println("[Speaker] 🚨 Playing emergency siren tone...");
    for (int i = 0; i < 3; i++) {
        playTone(880, 100);
        playTone(600, 100);
    }
}

void Speaker::playEmergencyMelody() {
    playTone(523,  120); // C5
    playTone(659,  120); // E5
    playTone(784,  120); // G5
    playTone(1047, 260); // C6
    playTone(784,  120); // G5
    playTone(659,  120); // E5
}

void Speaker::playEmergencyAlarm() {
    // Balanced 900 Hz / 700 Hz alternating emergency alarm warble
    playTone(900, 90);
    playTone(700, 90);
    playTone(900, 90);
    playTone(700, 90);
}

void Speaker::playNotReadyBeep() {
    // Modern dual-note rising status chirp (C5 523 Hz -> G5 784 Hz)
    playTone(523, 35);
    playTone(784, 45);
}

void Speaker::stop() {
    i2s_zero_dma_buffer(SPEAKER_I2S_PORT);
}
