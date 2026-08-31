#include <Arduino.h>
#include "app.h"
#include "../config/pins.h"
#include "../config/deviceConfig.h"
#include "../drivers/led.h"
#include "../drivers/button.h"
#include "../drivers/mic.h"
#include "../drivers/speaker.h"
#include "../drivers/dht.h"
#include "../drivers/gps.h"
#include "../services/wifiService.h"
#include "../services/webSocketService.h"

// ============================================================================
// HARDWARE
// ============================================================================

static Led    wifiLed(LED_PIN);
static Led    recordingLed(RECORDING_LED_PIN);
static Led    socketConnectedLed(SOCKET_CONNECTED_LED_PIN);
static Led    emergencyLed(EMERGENCY_BUTTON_LED_PIN);

static Button button(RECORDING_BUTTON_PIN);
static Button emergencyButton(EMERGENCY_BUTTON_PIN);

static Mic       mic;
static Speaker   speaker;
static DhtSensor dht;
static GpsSensor gps;

// ============================================================================
// SERVICES
// ============================================================================

static WifiService      wifi;
static WebSocketService webSocket;

// ============================================================================
// STATE — RECORDING
// ============================================================================

static bool isRecording = false;

// ============================================================================
// STATE — WiFi LED blink
// ============================================================================

static unsigned long lastBlink    = 0;
static bool          wifiLedState = false;

// ============================================================================
// STATE — Socket LED blink (when disconnected)
// ============================================================================

static unsigned long lastSocketBlink = 0;
static bool          socketLedState  = false;

// ============================================================================
// STATE — EMERGENCY
// ============================================================================

enum class EmergencyState { IDLE, HOLDING, TRIGGERED, CANCELLING };

static EmergencyState emergencyState     = EmergencyState::IDLE;
static unsigned long  emergencyHoldStart = 0;
static unsigned long  cancelHoldStart    = 0;
static unsigned long  lastEmergencyBlink = 0;
static unsigned long  lastEmergencyMelody = 0;
static bool           emergencyLedState  = false;

static const unsigned long EMERGENCY_HOLD_MS  = 5000;
static const unsigned long CANCEL_HOLD_MS     = 2000;
static const unsigned long EMERGENCY_BLINK_MS = 100;

// ============================================================================
// STATE — TELEMETRY
// ============================================================================

static unsigned long lastTelemetrySend = 0;
static const unsigned long TELEMETRY_INTERVAL_MS = 60000;   // 60 seconds

// Flag set by onWebSocketConnected so we send telemetry immediately on connect
static bool sendTelemetryOnConnect = false;

// ============================================================================
// STATE — DIAGNOSTIC LOGS
// ============================================================================

// DHT22: print summary every 12 s (= 4 reads × 3 s each)
static unsigned long lastDhtPrint = 0;
static const unsigned long DHT_PRINT_INTERVAL_MS = 12000;

// WebSocket heartbeat log (mirrors original 10 s behaviour)
static unsigned long lastWsStatus = 0;
static const unsigned long WS_STATUS_INTERVAL_MS = 10000;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void handleButtonAndRecording();
void handleEmergencyButton();
void handleTelemetry();
void handleNotReadyBeep();
void handleIntroPlayback();
void printDhtDiagnostic();
void printWsStatus();
void updateWifiLed();
void updateSocketLed();
void sendTelemetryNow(const char* reason);

// ============================================================================
// STATE — INTRO PLAYBACK
// ============================================================================

static unsigned long playIntroTimer   = 0;
static bool          playIntroPending = false;

// ============================================================================
// WEBSOCKET CALLBACKS
// ============================================================================

void onWebSocketConnected(const char* message) {
    Serial.println("[App] 🔗 WebSocket connected — sending TOKEN...");
    String tokenMsg = "{\"event\":\"TOKEN\",\"user_id\":\"";
    tokenMsg += DEVICE_ID;
    tokenMsg += "\"}";
    webSocket.sendMessage(tokenMsg.c_str());

    // Schedule immediate telemetry push after connect
    sendTelemetryOnConnect = true;

    // Play connection success chime
    speaker.playChime();

    // Schedule introduction playback 1 second after connect
    playIntroTimer   = millis();
    playIntroPending = true;
}

static bool isResponding = false;

void onWebSocketDisconnected(const char* message) {
    Serial.println("[App] ❌ WebSocket disconnected");

    isRecording = false;
    isResponding = false;
    recordingLed.off();

    if (emergencyState == EmergencyState::HOLDING) {
        emergencyState = EmergencyState::IDLE;
        emergencyLed.off();
    }

    sendTelemetryOnConnect = false;
    playIntroPending = false;
}

void onWebSocketMessage(const char* message) {
    Serial.printf("[App] 📨 Message: %s\n", message);
    if (strstr(message, "\"EMERGENCY_START\"") || strstr(message, "emergency_help")) {
        Serial.println("[App] 🚨 Voice Emergency SOS Activated — starting LED alarm & loop");
        speaker.stop();
        emergencyState     = EmergencyState::TRIGGERED;
        emergencyLedState  = false;
        lastEmergencyBlink = millis();
    } else if (strstr(message, "\"TTS_START\"")) {
        Serial.println("[App] 🔊 Incoming AI Speech Response — status LED SOLID ON & Jitter Buffer reset");
        speaker.stop();
        speaker.resetJitterBuffer();
        isResponding = true;
        recordingLed.on();
    } else if (strstr(message, "\"TTS_END\"")) {
        Serial.println("[App] ✅ AI Speech Response Stream Complete — status LED OFF");
        speaker.setPlaybackActive(false);
        isResponding = false;
        recordingLed.off();
    }
}

void onWebSocketBinary(const uint8_t* payload, size_t length) {
    speaker.write(payload, length);
}

void onWebSocketError(const char* message) {
    Serial.printf("[App] ⚠️ WebSocket error: %s\n", message);
}

// ============================================================================
// INIT
// ============================================================================

void App::init() {
    delay(1000);
    Serial.println("\n\n[App] 🚀 Initializing Synchora Device...");

    Serial.println("[App] 📡 Starting WiFi...");
    wifi.init();

    Serial.println("[App] 🔊 Starting Speaker (MAX98357A)...");
    speaker.init();
    speaker.startTask();

    Serial.println("[App] 🌡️  Starting DHT22...");
    dht.init();

    Serial.println("[App] 🛰️  Starting GPS...");
    gps.init();

    Serial.println("[App] 🎙️  Starting Mic...");
    mic.init();

    Serial.println("[App] 🔌 Starting WebSocket...");
    webSocket.onConnected(onWebSocketConnected);
    webSocket.onDisconnected(onWebSocketDisconnected);
    webSocket.onMessage(onWebSocketMessage);
    webSocket.onBinary(onWebSocketBinary);
    webSocket.onError(onWebSocketError);
    webSocket.init();

    emergencyLed.off();

    Serial.println("[App] ✅ Initialization complete!\n");
}

// ============================================================================
// LOOP
// ============================================================================

void App::run() {
    wifi.reconnectIfNeeded();
    webSocket.run();

    // Sensors — update every loop tick (both are internally throttled)
    dht.update();
    gps.update();

    // Features
    handleButtonAndRecording();
    handleEmergencyButton();
    handleTelemetry();
    handleNotReadyBeep();
    handleIntroPlayback();

    // Diagnostic logs
    printDhtDiagnostic();
    printWsStatus();

    // LEDs
    updateWifiLed();
    updateSocketLed();
}

// ============================================================================
// INTRO PLAYBACK — 1.0s after WebSocket connection
// ============================================================================

void handleIntroPlayback() {
    if (playIntroPending) {
        playIntroPending = false;
        Serial.println("[App] 🔊 WebSocket connected — requesting PLAY_INTRO from server immediately...");
        webSocket.sendMessage("{\"event\":\"PLAY_INTRO\"}");
    }
}

// ============================================================================
// NOT READY STATUS BEEP — plays soft status pip every 1.5s while connecting
// ============================================================================

static unsigned long lastNotReadyBeep = 0;

void handleNotReadyBeep() {
    // Silence status beep during active Emergency SOS or while AI is responding/recording
    if (emergencyState != EmergencyState::IDLE || isRecording || isResponding) return;

    bool isReady = wifi.isConnected() && webSocket.isConnected();
    if (!isReady) {
        unsigned long now = millis();
        if (now - lastNotReadyBeep >= 3000) {
            lastNotReadyBeep = now;
            speaker.playNotReadyBeep();
        }
    }
}

// ============================================================================
// RECORDING — socket-gated & VAD filtered with Emergency Lockout
// ============================================================================

static float computeRMS(const uint8_t* buffer, size_t bytes) {
    size_t samples = bytes / sizeof(int16_t);
    if (samples == 0) return 0.0f;
    const int16_t* pcm = (const int16_t*)buffer;
    float sum = 0.0f;
    for (size_t i = 0; i < samples; i++) {
        float sample = (float)pcm[i];
        sum += sample * sample;
    }
    return sqrtf(sum / (float)samples);
}

void handleButtonAndRecording() {
    // Voice recording locked out during active Emergency SOS alarm until user resets via 2s hold
    if (emergencyState != EmergencyState::IDLE) {
        if (isRecording) {
            isRecording = false;
            recordingLed.off();
        }
        return;
    }

    bool socketReady = webSocket.isConnected();

    if (button.isPressed()) {

        if (!socketReady) {
            if (isRecording) {
                Serial.println("[App] ⏹️ Socket lost — stopping recording");
                isRecording = false;
                recordingLed.off();
            }
            return;
        }

        // Barge-In Interrupt: Stop any active speech immediately if button is pressed
        if (isResponding || speaker.isBusy()) {
            Serial.println("[App] 🛑 Button pressed mid-speech — BARGE-IN INTERRUPT! Stopping speaker.");
            speaker.stop();
            isResponding = false;
            webSocket.sendMessage("{\"event\":\"CANCEL_SPEECH\"}");
        }

        if (!isRecording) {
            Serial.println("[App] 🎙️ Recording started");
            isRecording = true;
            recordingLed.on();
            webSocket.sendEvent("START");
        }

        uint8_t buffer[1024];
        int bytes = mic.read(buffer, 512);
        if (bytes > 0) {
            float rms = computeRMS(buffer, bytes);
            // Send binary packet if audio level exceeds VAD silence floor (250 RMS)
            if (rms >= 250.0f) {
                webSocket.sendBinary(buffer, bytes);
            }
        }

    } else {
        if (isRecording) {
            Serial.println("[App] ⏹️ Recording stopped — waiting for AI response");
            isRecording = false;
            isResponding = true;
            recordingLed.on(); // Keep solid ON while processing & receiving response
            if (socketReady) webSocket.sendEvent("END");
        } else if (!isResponding) {
            recordingLed.off();
        }
    }
}

// ============================================================================
// EMERGENCY BUTTON STATE MACHINE
// ============================================================================

void handleEmergencyButton() {
    bool btnHeld = emergencyButton.isPressed();
    unsigned long now = millis();
    static unsigned long lastReleaseTime = 0;

    if (btnHeld) {
        lastReleaseTime = now;
    }

    switch (emergencyState) {

        case EmergencyState::IDLE:
            if (btnHeld) {
                emergencyState     = EmergencyState::HOLDING;
                emergencyHoldStart = now;
                lastReleaseTime    = now;
                emergencyLed.on();
                Serial.println("[Emergency] 🟡 Button held — counting to 5 s...");
            } else {
                emergencyLed.off();
            }
            break;

        case EmergencyState::HOLDING:
            // Filter mechanical contact bounce: require continuous release >150ms before aborting
            if (!btnHeld && (now - lastReleaseTime >= 150)) {
                emergencyState = EmergencyState::IDLE;
                emergencyLed.off();
                Serial.println("[Emergency] ⬜ Released early — aborted.");
                break;
            }
            if (now - emergencyHoldStart >= EMERGENCY_HOLD_MS) {
                if (!webSocket.isConnected()) {
                    Serial.println("[Emergency] ❌ Socket not connected — cannot trigger!");
                    emergencyState = EmergencyState::IDLE;
                    emergencyLed.off();
                    break;
                }
                Serial.println("[Emergency] 🚨 TRIGGERED — sending EMERGENCY_TRIGGER");
                webSocket.sendMessage("{\"event\":\"EMERGENCY_TRIGGER\"}");
                speaker.stop();
                emergencyState     = EmergencyState::TRIGGERED;
                emergencyLedState  = false;
                lastEmergencyBlink = now;
            }
            break;

        case EmergencyState::TRIGGERED:
            // 4 blinks per second = 125 ms toggle interval
            if (now - lastEmergencyBlink >= 125) {
                lastEmergencyBlink = now;
                emergencyLedState  = !emergencyLedState;
                emergencyLedState ? emergencyLed.on() : emergencyLed.off();
            }

            if (btnHeld) {
                emergencyState  = EmergencyState::CANCELLING;
                cancelHoldStart = now;
                lastReleaseTime = now;
                Serial.println("[Emergency] 🔵 Cancel hold — counting to 2 s...");
            }
            break;

        case EmergencyState::CANCELLING:
            // Filter mechanical contact bounce: require continuous release >150ms before aborting
            if (!btnHeld && (now - lastReleaseTime >= 150)) {
                emergencyState     = EmergencyState::TRIGGERED;
                lastEmergencyBlink = now;
                Serial.println("[Emergency] 🔄 Released early — resuming blink.");
                break;
            }
            // 2 blinks per second = 250 ms toggle interval during cancel count
            if (now - lastEmergencyBlink >= 250) {
                lastEmergencyBlink = now;
                emergencyLedState  = !emergencyLedState;
                emergencyLedState ? emergencyLed.on() : emergencyLed.off();
            }

            if (now - cancelHoldStart >= CANCEL_HOLD_MS) {
                emergencyState = EmergencyState::IDLE;
                emergencyLed.off();
                speaker.stop();
                if (webSocket.isConnected()) {
                    webSocket.sendMessage("{\"event\":\"EMERGENCY_CANCEL\"}");
                }
                Serial.println("[Emergency] ✅ Cancelled — returning to normal.");
            }
            break;
    }
}

// ============================================================================
// TELEMETRY — 60 s interval + immediate on connect + event-driven GPS lock
// ============================================================================

void sendTelemetryNow(const char* reason) {
    if (!webSocket.isConnected()) return;

    float temp = dht.temperature();
    float hum  = dht.humidity();
    float lat  = gps.hasFix() ? gps.latitude()  : 0.0f;
    float lng  = gps.hasFix() ? gps.longitude() : 0.0f;

    // Build JSON manually — avoids pulling in ArduinoJson dependency
    char payload[200];
    snprintf(payload, sizeof(payload),
        "{\"event\":\"TELEMETRY_UPDATE\","
        "\"temperature\":%.2f,"
        "\"humidity\":%.2f,"
        "\"latitude\":%.6f,"
        "\"longitude\":%.6f}",
        temp, hum, lat, lng
    );

    Serial.printf("[Telemetry] 📤 Sending (%s) — T=%.1f°C  H=%.1f%%  Lat=%.6f  Lng=%.6f\n",
                  reason, temp, hum, lat, lng);

    webSocket.sendMessage(payload);
    lastTelemetrySend = millis();
}

void handleTelemetry() {
    unsigned long now = millis();

    // On-connect push (flag set by onWebSocketConnected)
    if (sendTelemetryOnConnect) {
        sendTelemetryOnConnect = false;
        sendTelemetryNow("on-connect");
        return;
    }

    // Event-driven: GPS just got its first fix this session
    if (gps.justGotFix()) {
        sendTelemetryNow("gps-lock");
        return;
    }

    // Regular 60 s interval
    if (now - lastTelemetrySend >= TELEMETRY_INTERVAL_MS) {
        sendTelemetryNow("interval-60s");
    }
}

// ============================================================================
// DIAGNOSTIC — DHT22 summary every 12 s
// ============================================================================

void printDhtDiagnostic() {
    unsigned long now = millis();
    if (now - lastDhtPrint < DHT_PRINT_INTERVAL_MS) return;
    lastDhtPrint = now;

    Serial.println("┌─────────────────────────────────┐");
    Serial.println("│         DHT22 Diagnostic         │");

    if (dht.hasValidData()) {
        Serial.printf( "│  Temperature : %6.1f °C         │\n", dht.temperature());
        Serial.printf( "│  Humidity    : %6.1f %%          │\n", dht.humidity());
    } else {
        Serial.println("│  ⚠️  No valid reading yet        │");
    }

    Serial.printf(  "│  GPS Fix     : %s              │\n",
                    gps.hasFix() ? "✅ YES" : "❌ NO ");
    if (gps.hasFix()) {
        Serial.printf("│  Sats        : %-3d               │\n", gps.satellites());
        Serial.printf("│  Lat         : %10.6f       │\n", gps.latitude());
        Serial.printf("│  Lng         : %10.6f       │\n", gps.longitude());
    } else {
        Serial.printf("│  Sats in view: %-3d               │\n", gps.satellites());
    }

    Serial.println("└─────────────────────────────────┘");
}

// ============================================================================
// DIAGNOSTIC — WebSocket heartbeat every 10 s
// ============================================================================

void printWsStatus() {
    unsigned long now = millis();
    if (now - lastWsStatus < WS_STATUS_INTERVAL_MS) return;
    lastWsStatus = now;

    Serial.printf("[WebSocket] State: %s  WiFi: %s\n",
        webSocket.isConnected() ? "CONNECTED" : "DISCONNECTED",
        wifi.isConnected()      ? "OK"        : "DOWN"
    );
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
        socketConnectedLed.on();
    } else {
        if (millis() - lastSocketBlink > 500) {
            lastSocketBlink = millis();
            socketLedState  = !socketLedState;
            socketLedState ? socketConnectedLed.on() : socketConnectedLed.off();
        }
    }
}