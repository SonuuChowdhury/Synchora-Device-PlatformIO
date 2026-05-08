#pragma once
#include <Arduino.h>

class GpsSensor {
public:
    void  init();
    void  update();          // feed GPS bytes — call EVERY loop iteration

    bool  hasFix();          // true when satellites locked
    float latitude();
    float longitude();
    int   satellites();

    // Returns true exactly ONCE when fix is first acquired this session.
    // Caller must consume this flag — it auto-resets after one call.
    bool  justGotFix();

private:
    float        _lat           = 0.0f;
    float        _lng           = 0.0f;
    int          _satellites    = 0;
    bool         _hasFix        = false;
    bool         _prevHasFix    = false;   // to detect fix transition
    bool         _justGotFix    = false;   // one-shot flag
};