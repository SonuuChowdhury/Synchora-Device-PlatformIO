#include <Arduino.h>
#include "app.h"
#include "../config/pins.h"
#include "../drivers/led.h"
#include "../drivers/button.h"
#include "../services/wifiService.h"
#include "../services/webSocketService.h"

// ============================================================================
// HARDWARE INITIALIZATION
// ============================================================================

static Led wifiLed(LED_PIN);                
static Led recordingLed(RECORDING_LED_PIN); 
static Led socketConnectedLed(SOCKET_CONNECTED_LED_PIN);
static Button button(RECORDING_BUTTON_PIN);  

// ============================================================================
// SERVICES
// ============================================================================

static WifiService wifi;
static WebSocketService webSocket;

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

const char* DEVICE_ID = "synchora84205@!&100@!%device";
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
    
    // Send authentication token
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
    
    // Parse and handle different event types from server
    // Example: {"status":"ok"} or {"error":"..."}
    // Add your custom logic here
}

void onWebSocketError(const char* message) {
    Serial.printf("[App] ⚠️ WebSocket error: %s\n", message);
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void App::init(){
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n[App] 🚀 Initializing Synchora Device...");

    Serial.println("[App] 📡 Starting WiFi...");
    wifi.init(); // now blocks until connected

    Serial.println("[App] 🔌 Starting WebSocket...");
    webSocket.onConnected(onWebSocketConnected);
    webSocket.onDisconnected(onWebSocketDisconnected);
    webSocket.onMessage(onWebSocketMessage);
    webSocket.onError(onWebSocketError);
    webSocket.init(); // safe to call now — DNS will resolve

    Serial.println("[App] ✅ Initialization complete!");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void App::run(){
    // 🌐 Update WiFi and WebSocket services
    webSocket.run();

    // 🔘 BUTTON & RECORDING LED
    handleButtonAndRecording();

    // 💡 WiFi LED Status
    updateWifiLed();

    // 💡 WebSocket LED Status
    updateSocketLed();
}

// ============================================================================
// LED & BUTTON HANDLERS
// ============================================================================

void handleButtonAndRecording() {
    if (button.isPressed()) {
        if (!isRecording) {
            Serial.println("[App] 🎙️ Recording started");
            isRecording = true;
            recordingLed.on();
            
            // Send START event to server
            webSocket.sendEvent("START");
        }
    } else {
        if (isRecording) {
            Serial.println("[App] ⏹️ Recording stopped");
            isRecording = false;
            recordingLed.off();
            
            // Send END event to server
            webSocket.sendEvent("END");
        }
    }
}

void updateWifiLed() {
    if (wifi.isConnected()) {
        wifiLed.on(); // WiFi connected - solid ON
    } else {
        // WiFi disconnected - blink every 500ms
        if (millis() - lastBlink > 500) {
            lastBlink = millis();
            wifiLedState = !wifiLedState;
            wifiLedState ? wifiLed.on() : wifiLed.off();
        }
    }
}

void updateSocketLed() {
    if (webSocket.isConnected()) {
        socketConnectedLed.on(); // WebSocket connected - solid ON
    } else {
        // WebSocket disconnected - blink every 500ms
        if (millis() - lastSocketBlink > 500) {
            lastSocketBlink = millis();
            socketLedState = !socketLedState;
            socketLedState ? socketConnectedLed.on() : socketConnectedLed.off();
        }
    }
}