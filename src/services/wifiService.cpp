#include <WiFi.h>
#include "../config/wifiConfig.h"
#include "wifiService.h"

void WifiService::init() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("[WiFi] Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n[WiFi] ✅ Connected! IP: %s\n", WiFi.localIP().toString().c_str());
}

bool WifiService::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void WifiService::reconnectIfNeeded() {
    static unsigned long lastCheck = 0;

    // Only check every 5 seconds to avoid hammering WiFi stack
    if (millis() - lastCheck < 5000) return;
    lastCheck = millis();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] ⚠️ Connection lost — reconnecting...");
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASS);

        // Wait up to 10 seconds for reconnect
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\n[WiFi] ✅ Reconnected! IP: %s\n", WiFi.localIP().toString().c_str());
        } else {
            Serial.println("\n[WiFi] ❌ Reconnect failed, will retry...");
        }
    }
}