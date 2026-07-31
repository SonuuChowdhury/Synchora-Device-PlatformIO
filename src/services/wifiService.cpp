#include <WiFi.h>
#include "../config/wifiConfig.h"
#include "wifiService.h"

void WifiService::init() {
    Serial.printf("[WiFi] 📡 Connecting to SSID: %s ...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
}

bool WifiService::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void WifiService::reconnectIfNeeded() {
    static unsigned long lastCheck = 0;
    static bool wasConnected = false;

    bool currentlyConnected = (WiFi.status() == WL_CONNECTED);

    if (currentlyConnected && !wasConnected) {
        wasConnected = true;
        Serial.printf("\n[WiFi] ✅ Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else if (!currentlyConnected && wasConnected) {
        wasConnected = false;
        Serial.println("\n[WiFi] ⚠️ Connection lost!");
    }

    // Check reconnection every 10 seconds if disconnected
    if (!currentlyConnected) {
        if (millis() - lastCheck > 10000) {
            lastCheck = millis();
            Serial.println("[WiFi] 🔄 Attempting reconnect...");
            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASS);
        }
    }
}