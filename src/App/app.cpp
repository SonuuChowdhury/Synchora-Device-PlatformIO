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
static Led emergencyLed(EMERGENCY_BUTTON_LED_PIN);

static Button button(RECORDING_BUTTON_PIN);
static Button emergencyButton(EMERGENCY_BUTTON_PIN);

static Mic mic;

// ============================================================================
// SERVICES
// ============================================================================

static WifiService wifi;
static WebSocketService webSocket;

// ============================================================================
// STATE — RECORDING
// ============================================================================

bool isRecording = false;

// ============================================================================
// STATE — WIFI LED BLINK
// ============================================================================

unsigned long lastBlink     = 0;
bool          wifiLedState  = false;

// ============================================================================
// STATE — SOCKET LED BLINK (when disconnected)
// ============================================================================

unsigned long lastSocketBlink = 0;
bool          socketLedState  = false;

// ============================================================================
// STATE — EMERGENCY
//
//  IDLE        → button not held, led off (or solid while counting)
//  HOLDING     → button held, led ON solid, counting toward 5 s trigger
//  TRIGGERED   → emergency sent, led fast-blink indefinitely
//  CANCELLING  → while TRIGGERED, button held again counting toward 2 s cancel
// ============================================================================

enum class EmergencyState {
    IDLE,
    HOLDING,
    TRIGGERED,
    CANCELLING
};

static EmergencyState emergencyState    = EmergencyState::IDLE;
static unsigned long  emergencyHoldStart = 0;   // when button press began
static unsigned long  cancelHoldStart    = 0;   // when cancel press began
static unsigned long  lastEmergencyBlink = 0;   // for fast-blink timing
static bool           emergencyLedState  = false;

// Tuning constants
static const unsigned long EMERGENCY_HOLD_MS  = 5000;  // hold to trigger
static const unsigned long CANCEL_HOLD_MS     = 2000;  // hold to cancel
static const unsigned long EMERGENCY_BLINK_MS = 100;   // fast-blink period

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void handleButtonAndRecording();
void handleEmergencyButton();
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

    // Stop recording if we lose the socket mid-session
    isRecording = false;
    recordingLed.off();

    // If emergency was being held, reset it — socket is gone anyway
    if (emergencyState == EmergencyState::HOLDING) {
        emergencyState = EmergencyState::IDLE;
        emergencyLed.off();
    }
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
    // Serial.begin already called in main.cpp
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

    // Ensure emergency LED starts off
    emergencyLed.off();

    Serial.println("[App] ✅ Initialization complete!");
}

// ============================================================================
// LOOP
// ============================================================================

void App::run() {
    wifi.reconnectIfNeeded();
    webSocket.run();

    handleButtonAndRecording();
    handleEmergencyButton();

    updateWifiLed();
    updateSocketLed();
}

// ============================================================================
// RECORDING — only active when socket is fully connected
// ============================================================================

void handleButtonAndRecording() {

    // Guard: recording is only allowed when the socket LED is solid ON,
    // i.e. the WebSocket is fully connected.
    bool socketReady = webSocket.isConnected();

    if (button.isPressed()) {

        if (!socketReady) {
            // Socket not ready — ignore button, make sure we are not recording
            if (isRecording) {
                Serial.println("[App] ⏹️ Socket lost — stopping recording");
                isRecording = false;
                recordingLed.off();
                // Do NOT send END, socket is gone
            }
            return;
        }

        // Socket is ready and button is pressed
        if (!isRecording) {
            Serial.println("[App] 🎙️ Recording started");
            isRecording = true;
            recordingLed.on();
            webSocket.sendEvent("START");
        }

        // Stream audio while button is held
        uint8_t buffer[1024];
        int bytes = mic.read(buffer, 512);
        if (bytes > 0) {
            webSocket.sendBinary(buffer, bytes);
        }

    } else {

        if (isRecording) {
            Serial.println("[App] ⏹️ Recording stopped");
            isRecording = false;
            recordingLed.off();
            if (socketReady) {
                webSocket.sendEvent("END");
            }
        }
    }
}

// ============================================================================
// EMERGENCY BUTTON STATE MACHINE
//
//  IDLE
//    button pressed  → move to HOLDING, note holdStart, led ON solid
//    button released → stay IDLE, led off
//
//  HOLDING
//    button still held AND elapsed >= 5 s AND socket connected
//      → send EMERGENCY_TRIGGER, move to TRIGGERED, led begins fast-blink
//    button still held AND elapsed >= 5 s AND socket NOT connected
//      → Serial warning, return to IDLE, led off
//    button released before 5 s → return to IDLE, led off
//
//  TRIGGERED
//    button pressed → move to CANCELLING, note cancelStart
//    button not pressed → fast-blink led
//
//  CANCELLING
//    button still held AND elapsed >= 2 s → return to IDLE, led off
//    button released before 2 s → return to TRIGGERED, resume fast-blink
// ============================================================================

void handleEmergencyButton() {

    bool btnHeld = emergencyButton.isPressed();
    unsigned long now = millis();

    switch (emergencyState) {

        // ── IDLE ──────────────────────────────────────────────────────────
        case EmergencyState::IDLE:

            if (btnHeld) {
                emergencyState    = EmergencyState::HOLDING;
                emergencyHoldStart = now;
                emergencyLed.on();   // solid ON while counting
                Serial.println("[Emergency] 🟡 Button held — counting to 5 s...");
            } else {
                emergencyLed.off();
            }
            break;

        // ── HOLDING ───────────────────────────────────────────────────────
        case EmergencyState::HOLDING:

            if (!btnHeld) {
                // Released before 5 s — abort
                emergencyState = EmergencyState::IDLE;
                emergencyLed.off();
                Serial.println("[Emergency] ⬜ Button released early — aborted.");
                break;
            }

            // Still held — check elapsed time
            if (now - emergencyHoldStart >= EMERGENCY_HOLD_MS) {

                if (!webSocket.isConnected()) {
                    Serial.println("[Emergency] ❌ Socket not connected — cannot trigger!");
                    emergencyState = EmergencyState::IDLE;
                    emergencyLed.off();
                    break;
                }

                // Trigger!
                Serial.println("[Emergency] 🚨 5 s hold complete — triggering emergency!");
                webSocket.sendMessage("{\"event\":\"EMERGENCY_TRIGGER\"}");

                emergencyState     = EmergencyState::TRIGGERED;
                emergencyLedState  = false;
                lastEmergencyBlink = now;
                // LED will start fast-blinking in TRIGGERED block below
            }
            // else: still counting, LED remains solid ON
            break;

        // ── TRIGGERED ─────────────────────────────────────────────────────
        case EmergencyState::TRIGGERED:

            // Fast-blink the emergency LED
            if (now - lastEmergencyBlink >= EMERGENCY_BLINK_MS) {
                lastEmergencyBlink = now;
                emergencyLedState  = !emergencyLedState;
                emergencyLedState ? emergencyLed.on() : emergencyLed.off();
            }

            // Watch for cancel press
            if (btnHeld) {
                emergencyState  = EmergencyState::CANCELLING;
                cancelHoldStart = now;
                Serial.println("[Emergency] 🔵 Cancel hold detected — counting to 2 s...");
            }
            break;

        // ── CANCELLING ────────────────────────────────────────────────────
        case EmergencyState::CANCELLING:

            if (!btnHeld) {
                // Released before 2 s — go back to fast-blinking
                emergencyState     = EmergencyState::TRIGGERED;
                lastEmergencyBlink = now;
                Serial.println("[Emergency] 🔄 Cancel released early — resuming blink.");
                break;
            }

            // Still held for cancel — keep fast-blinking during count
            if (now - lastEmergencyBlink >= EMERGENCY_BLINK_MS) {
                lastEmergencyBlink = now;
                emergencyLedState  = !emergencyLedState;
                emergencyLedState ? emergencyLed.on() : emergencyLed.off();
            }

            if (now - cancelHoldStart >= CANCEL_HOLD_MS) {
                // Cancel confirmed — return to IDLE
                emergencyState = EmergencyState::IDLE;
                emergencyLed.off();
                Serial.println("[Emergency] ✅ Emergency cancelled — returning to normal.");
            }
            break;
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
            lastBlink    = millis();
            wifiLedState = !wifiLedState;
            wifiLedState ? wifiLed.on() : wifiLed.off();
        }
    }
}

void updateSocketLed() {
    if (webSocket.isConnected()) {
        socketConnectedLed.on();    // solid ON = socket ready, recording allowed
    } else {
        if (millis() - lastSocketBlink > 500) {
            lastSocketBlink = millis();
            socketLedState  = !socketLedState;
            socketLedState ? socketConnectedLed.on() : socketConnectedLed.off();
        }
    }
}