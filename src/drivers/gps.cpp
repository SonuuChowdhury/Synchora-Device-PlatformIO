#include "gps.h"
#include "../config/pins.h"
#include <TinyGPSPlus.h>

// TinyGPSPlus parser instance — file-scoped
static TinyGPSPlus gpsParser;

// Hardware serial for NEO-6M
// ESP32: Serial2 maps to UART2 (configurable pins)
static HardwareSerial gpsSerial(2);

static const uint32_t GPS_BAUD = 9600;  // NEO-6M default baud rate

void GpsSensor::init() {
    // Begin Serial2 with custom RX/TX pins from pins.h
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    Serial.printf("[GPS] ✅ NEO-6M initialized — RX:%d TX:%d @ %d baud\n",
                  GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);
    Serial.println("[GPS] ⏳ Waiting for satellite fix...");
}

void GpsSensor::update() {
    // Drain all available bytes into the parser this loop tick
    // TinyGPSPlus handles NMEA parsing incrementally — never blocks
    while (gpsSerial.available() > 0) {
        char c = gpsSerial.read();
        bool newSentence = gpsParser.encode(c);

        // Only process when a complete NMEA sentence has been parsed
        if (newSentence) {

            if (gpsParser.location.isValid() && gpsParser.location.isUpdated()) {
                _lat        = (float)gpsParser.location.lat();
                _lng        = (float)gpsParser.location.lng();
                _satellites = (int)gpsParser.satellites.value();
                _hasFix     = true;

                // Print every GPS update to serial so you can see live data
                Serial.printf(
                    "[GPS] 📡 Fix: Lat=%.6f  Lng=%.6f  Sats=%d  Age=%lums\n",
                    _lat, _lng, _satellites,
                    gpsParser.location.age()
                );
            }

            // Satellites count even without a position fix
            if (gpsParser.satellites.isValid()) {
                _satellites = (int)gpsParser.satellites.value();
                if (!_hasFix) {
                    Serial.printf("[GPS] 🔍 Searching... Satellites in view: %d\n",
                                  _satellites);
                }
            }
        }
    }

    // Detect the no-fix → fix transition for the one-shot event-driven push
    bool currentFix = _hasFix;
    if (currentFix && !_prevHasFix) {
        _justGotFix = true;   // caller will consume this
        Serial.printf(
            "[GPS] 🎯 FIRST FIX ACQUIRED — Lat=%.6f  Lng=%.6f  Sats=%d\n",
            _lat, _lng, _satellites
        );
    }
    _prevHasFix = currentFix;
}

bool  GpsSensor::hasFix()     { return _hasFix;      }
float GpsSensor::latitude()   { return _lat;          }
float GpsSensor::longitude()  { return _lng;          }
int   GpsSensor::satellites() { return _satellites;   }

bool GpsSensor::justGotFix() {
    if (_justGotFix) {
        _justGotFix = false;  // auto-reset — one-shot
        return true;
    }
    return false;
}