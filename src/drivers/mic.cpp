#include "mic.h"
#include "../config/pins.h"
#include <driver/i2s.h>

#define I2S_PORT        I2S_NUM_0
#define SAMPLE_RATE     16000
#define DMA_BUF_COUNT   8
#define DMA_BUF_LEN     512

// ============================================================================
// INIT
// ============================================================================

void Mic::init() {
    i2s_config_t config = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = DMA_BUF_COUNT,
        .dma_buf_len          = DMA_BUF_LEN,
        .use_apll             = false,
        .tx_desc_auto_clear   = false,
        .fixed_mclk           = 0
    };

    i2s_pin_config_t pins = {
        .bck_io_num   = MIC_SCK_PIN,
        .ws_io_num    = MIC_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = MIC_SD_PIN
    };

    esp_err_t err;

    err = i2s_driver_install(I2S_PORT, &config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[Mic] ❌ i2s_driver_install failed: %d\n", err);
        return;
    }

    err = i2s_set_pin(I2S_PORT, &pins);
    if (err != ESP_OK) {
        Serial.printf("[Mic] ❌ i2s_set_pin failed: %d\n", err);
        return;
    }

    err = i2s_set_clk(I2S_PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_MONO);
    if (err != ESP_OK) {
        Serial.printf("[Mic] ❌ i2s_set_clk failed: %d\n", err);
        return;
    }

    i2s_zero_dma_buffer(I2S_PORT);

    Serial.println("[Mic] ✅ INMP441 initialized");

    test();
}

// ============================================================================
// CONNECTION TEST
// ============================================================================

void Mic::test() {
    Serial.println("[Mic] 🔍 Running connection test...");
    delay(100);

    int32_t testBuf[64];
    size_t bytesRead = 0;

    i2s_read(I2S_PORT, testBuf, sizeof(testBuf), &bytesRead, pdMS_TO_TICKS(1000));

    int samples    = bytesRead / 4;
    int nonZero    = 0;
    int32_t maxVal = 0;
    int32_t minVal = 0;

    for (int i = 0; i < samples; i++) {
        if (testBuf[i] != 0) nonZero++;
        if (testBuf[i] > maxVal) maxVal = testBuf[i];
        if (testBuf[i] < minVal) minVal = testBuf[i];
    }

    float nonZeroPct = (float)nonZero / samples * 100.0f;

    Serial.printf("[Mic] 📊 Samples read  : %d\n", samples);
    Serial.printf("[Mic] 📊 Non-zero      : %d (%.1f%%)\n", nonZero, nonZeroPct);
    Serial.printf("[Mic] 📊 Max raw value : %ld\n", (long)maxVal);
    Serial.printf("[Mic] 📊 Min raw value : %ld\n", (long)minVal);

    Serial.print("[Mic] 📊 Raw samples   : ");
    for (int i = 0; i < 8 && i < samples; i++) {
        Serial.printf("%ld ", (long)testBuf[i]);
    }
    Serial.println();

    if (nonZero == 0) {
        Serial.println("[Mic] ❌ TEST FAILED — All samples are zero");
        Serial.println("[Mic]    → Check SD pin and L/R pin");
    } else if (nonZeroPct < 10.0f) {
        Serial.println("[Mic] ⚠️  TEST WARNING — Very few non-zero samples");
        Serial.println("[Mic]    → Check L/R pin is connected to GND");
    } else {
        Serial.println("[Mic] ✅ TEST PASSED — Mic is connected and responding");
    }
}

// ============================================================================
// READ
// ============================================================================

int Mic::read(uint8_t* buffer, size_t numSamples) {
    int32_t raw[512];
    size_t bytesRead = 0;

    i2s_read(I2S_PORT, raw, numSamples * sizeof(int32_t), &bytesRead, portMAX_DELAY);

    int samplesRead = bytesRead / sizeof(int32_t);
    int16_t* out = (int16_t*)buffer;

    for (int i = 0; i < samplesRead; i++) {
        int32_t s = raw[i] >> 14;
        if (s >  32767) s =  32767;
        if (s < -32768) s = -32768;
        out[i] = (int16_t)s;
    }

    return samplesRead * sizeof(int16_t);
}