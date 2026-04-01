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