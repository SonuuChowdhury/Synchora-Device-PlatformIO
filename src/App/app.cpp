#include <Arduino.h>
#include "app.h"
#include "../config/pins.h"
#include "../config/deviceConfig.h"
#include "../drivers/led.h"
#include "../drivers/button.h"
#include "../drivers/mic.h"
#include "../services/wifiService.h"
#include "../services/webSocketService.h"

// ============================================================================
// HARDWARE
// ============================================================================

static Led wifiLed(LED_PIN);
static Led recordingLed(RECORDING_LED_PIN);
static Led socketConnectedLed(SOCKET_CONNECTED_LED_PIN);
static Button button(RECORDING_BUTTON_PIN);
static Mic mic;

// ============================================================================
// SERVICES
// ============================================================================

static WifiService wifi;
static WebSocketService webSocket;

// ============================================================================
// STATE
// ============================================================================

bool isRecording = false;

unsigned long lastBlink = 0;
unsigned long lastSocketBlink = 0;
bool wifiLedState = false;
bool socketLedState = false;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void handleButtonAndRecording();
void updateWifiLed();
void updateSocketLed();

// ============================================================================
// WEBSOCKET CALLBACKS
// ============================================================================

void onWebSocketConnected(const char* message) {
    Serial.println("[App] 🔗 WebSocket connected - Sending TOKEN...");
    String tokenMsg = "{\"event\":\"TOKEN\",\"user_id\":\"";
    tokenMsg += DEVICE_ID;
    tokenMsg += "\"}";
    webSocket.sendMessage(tokenMsg.c_str());
}

void onWebSocketDisconnected(const char* message) {
    Serial.println("[App] ❌ WebSocket disconnected");
    isRecording = false;
    recordingLed.off();
}

void onWebSocketMessage(const char* message) {
    Serial.printf("[App] 📨 Message received: %s\n", message);
}

void onWebSocketError(const char* message) {
    Serial.printf("[App] ⚠️ WebSocket error: %s\n", message);
}

// ============================================================================
// INIT
// ============================================================================

void App::init() {
    // Serial.begin already called in main.cpp — removed duplicate here
    delay(1000);
    Serial.println("\n\n[App] 🚀 Initializing Synchora Device...");

    Serial.println("[App] 📡 Starting WiFi...");
    wifi.init();

    Serial.println("[App] 🎙️ Starting Mic...");
    mic.init();

    Serial.println("[App] 🔌 Starting WebSocket...");
    webSocket.onConnected(onWebSocketConnected);
    webSocket.onDisconnected(onWebSocketDisconnected);
    webSocket.onMessage(onWebSocketMessage);
    webSocket.onError(onWebSocketError);
    webSocket.init();

    Serial.println("[App] ✅ Initialization complete!");
}

// ============================================================================
// LOOP
// ============================================================================

void App::run() {
    wifi.reconnectIfNeeded(); // handles WiFi drop and reconnect
    webSocket.run();
    handleButtonAndRecording();
    updateWifiLed();
    updateSocketLed();
}

// ============================================================================
// BUTTON & RECORDING
// ============================================================================

void handleButtonAndRecording() {
    if (button.isPressed()) {
        if (!isRecording) {
            Serial.println("[App] 🎙️ Recording started");
            isRecording = true;
            recordingLed.on();
            webSocket.sendEvent("START");
        }

        // Stream audio while button is held
        // mic.read() reads numSamples x 32-bit samples from I2S,
        // converts to 16-bit, returns byte count of output (numSamples x 2 bytes)
        if (webSocket.isConnected()) {
            uint8_t buffer[1024]; // holds 512 x int16_t samples
            int bytes = mic.read(buffer, 512); // 512 = number of 32-bit raw samples to read
            if (bytes > 0) {
                webSocket.sendBinary(buffer, bytes);
            }
        }

    } else {
        if (isRecording) {
            Serial.println("[App] ⏹️ Recording stopped");
            isRecording = false;
            recordingLed.off();
            webSocket.sendEvent("END");
        }
    }
}

// ============================================================================
// LED HANDLERS
// ============================================================================

void updateWifiLed() {
    if (wifi.isConnected()) {
        wifiLed.on();
    } else {
        if (millis() - lastBlink > 500) {
            lastBlink = millis();
            wifiLedState = !wifiLedState;
            wifiLedState ? wifiLed.on() : wifiLed.off();
        }
    }
}

void updateSocketLed() {
    if (webSocket.isConnected()) {
        socketConnectedLed.on();
    } else {
        if (millis() - lastSocketBlink > 500) {
            lastSocketBlink = millis();
            socketLedState = !socketLedState;
            socketLedState ? socketConnectedLed.on() : socketConnectedLed.off();
        }
    }
}