#include "speaker.h"
#include "../config/pins.h"
#include <driver/i2s.h>
#include <math.h>

#define SPEAKER_I2S_PORT   I2S_NUM_1
#define SAMPLE_RATE        16000
#define DMA_BUF_COUNT      16
#define DMA_BUF_LEN        1024

#define RING_BUF_SIZE      16384

static uint8_t ringBuf[RING_BUF_SIZE];
static volatile size_t ringHead = 0;
static volatile size_t ringTail = 0;
static portMUX_TYPE ringMux = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t speakerTaskHandle = NULL;

static void speakerTask(void* param) {
    uint8_t chunkBuf[512];
    for (;;) {
        size_t toRead = 0;

        portENTER_CRITICAL(&ringMux);
        size_t available = (ringHead >= ringTail) 
            ? (ringHead - ringTail) 
            : (RING_BUF_SIZE - ringTail + ringHead);

        if (available > 0) {
            toRead = available > sizeof(chunkBuf) ? sizeof(chunkBuf) : available;
            for (size_t i = 0; i < toRead; i++) {
                chunkBuf[i] = ringBuf[ringTail];
                ringTail = (ringTail + 1) % RING_BUF_SIZE;
            }
        }
        portEXIT_CRITICAL(&ringMux);

        if (toRead > 0) {
            size_t bytesWritten = 0;
            i2s_write(SPEAKER_I2S_PORT, chunkBuf, toRead, &bytesWritten, pdMS_TO_TICKS(50));
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

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
        Serial.printf("[Speaker] i2s_driver_install failed: %d\n", err);
        return;
    }

    err = i2s_set_pin(SPEAKER_I2S_PORT, &pins);
    if (err != ESP_OK) {
        Serial.printf("[Speaker] i2s_set_pin failed: %d\n", err);
        return;
    }

    err = i2s_set_clk(SPEAKER_I2S_PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
    if (err != ESP_OK) {
        Serial.printf("[Speaker] i2s_set_clk failed: %d\n", err);
        return;
    }

    i2s_zero_dma_buffer(SPEAKER_I2S_PORT);

    Serial.println("[Speaker] MAX98357A I2S Amplifier initialized");
}

void Speaker::startTask() {
    if (speakerTaskHandle == NULL) {
        xTaskCreatePinnedToCore(
            speakerTask,
            "SpeakerTask",
            4096,
            NULL,
            2,
            &speakerTaskHandle,
            0 // Pin to Core 0 so main app loop runs freely on Core 1
        );
        Serial.println("[Speaker] FreeRTOS speaker task started on Core 0");
    }
}

void Speaker::write(const uint8_t* buffer, size_t size) {
    if (speakerTaskHandle == NULL) {
        size_t bytesWritten = 0;
        i2s_write(SPEAKER_I2S_PORT, buffer, size, &bytesWritten, pdMS_TO_TICKS(100));
        return;
    }

    for (size_t i = 0; i < size; i++) {
        portENTER_CRITICAL(&ringMux);
        size_t nextHead = (ringHead + 1) % RING_BUF_SIZE;
        if (nextHead != ringTail) {
            ringBuf[ringHead] = buffer[i];
            ringHead = nextHead;
        } else {
            // Buffer full, drop oldest byte
            ringTail = (ringTail + 1) % RING_BUF_SIZE;
            ringBuf[ringHead] = buffer[i];
            ringHead = nextHead;
        }
        portEXIT_CRITICAL(&ringMux);
    }
}

bool Speaker::isBusy() {
    portENTER_CRITICAL(&ringMux);
    bool busy = (ringHead != ringTail);
    portEXIT_CRITICAL(&ringMux);
    return busy;
}

void Speaker::playTone(uint32_t frequencyHz, uint32_t durationMs) {
    size_t numFrames = (SAMPLE_RATE * durationMs) / 1000;
    if (numFrames == 0) return;

    int16_t* buffer = (int16_t*)malloc(numFrames * 2 * sizeof(int16_t));
    if (!buffer) return;

    float amplitude = 18000.0f;
    size_t fadeSamples = (SAMPLE_RATE * 8) / 1000;
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

        buffer[2 * i]     = sampleVal;
        buffer[2 * i + 1] = sampleVal;
    }

    write((const uint8_t*)buffer, numFrames * 2 * sizeof(int16_t));
    free(buffer);
}

void Speaker::playChime() {
    Serial.println("[Speaker] Playing startup/connection chime...");
    playTone(523, 100);
    playTone(659, 100);
    playTone(784, 180);
}

void Speaker::playSiren() {
    Serial.println("[Speaker] Playing emergency siren tone...");
    for (int i = 0; i < 3; i++) {
        playTone(880, 100);
        playTone(600, 100);
    }
}

void Speaker::playEmergencyMelody() {
    playTone(523,  120);
    playTone(659,  120);
    playTone(784,  120);
    playTone(1047, 260);
    playTone(784,  120);
    playTone(659,  120);
}

void Speaker::playEmergencyAlarm() {
    playTone(900, 90);
    playTone(700, 90);
    playTone(900, 90);
    playTone(700, 90);
}

void Speaker::playNotReadyBeep() {
    playTone(523, 40);
    playTone(784, 50);
}

void Speaker::stop() {
    portENTER_CRITICAL(&ringMux);
    ringHead = 0;
    ringTail = 0;
    portEXIT_CRITICAL(&ringMux);
    i2s_zero_dma_buffer(SPEAKER_I2S_PORT);
}
