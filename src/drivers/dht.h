#pragma once
#include <Arduino.h>

class DhtSensor {
public:
    void  init();
    void  update();          // call every loop — internally throttled to 3 s
    float temperature();     // last valid reading, NaN-safe
    float humidity();        // last valid reading, NaN-safe
    bool  hasValidData();    // true once at least one good read happened

private:
    float        _temp          = 0.0f;
    float        _hum           = 0.0f;
    bool         _valid         = false;
    unsigned long _lastReadMs   = 0;

    static const unsigned long READ_INTERVAL_MS = 3000;
};