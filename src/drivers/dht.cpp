#include "dht.h"
#include "../config/pins.h"
#include <DHT.h>

// DHT22 sensor instance — file-scoped, not exposed in header
static DHT dht(DHT22_PIN, DHT22);

void DhtSensor::init() {
    dht.begin();
    Serial.println("[DHT22] ✅ Initialized on pin " + String(DHT22_PIN));
}

void DhtSensor::update() {
    unsigned long now = millis();

    // Enforce minimum 3 s between reads — DHT22 hardware requirement
    if (now - _lastReadMs < READ_INTERVAL_MS) return;
    _lastReadMs = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    // Validate — isnan() catches failed reads from the sensor
    if (isnan(t) || isnan(h)) {
        Serial.println("[DHT22] ⚠️  Read failed — keeping last valid value");
        return;   // _temp and _hum unchanged, _valid unchanged
    }

    // Sanity range check — DHT22 spec: -40 to 80 °C, 0–100 %RH
    if (t < -40.0f || t > 80.0f || h < 0.0f || h > 100.0f) {
        Serial.printf("[DHT22] ⚠️  Out-of-range reading (T=%.1f H=%.1f) — discarding\n", t, h);
        return;
    }

    _temp  = t;
    _hum   = h;
    _valid = true;
}

float DhtSensor::temperature() { return _temp; }
float DhtSensor::humidity()    { return _hum;  }
bool  DhtSensor::hasValidData(){ return _valid; }