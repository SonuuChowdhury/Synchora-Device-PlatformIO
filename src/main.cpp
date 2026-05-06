#include <Arduino.h>
#include "app/app.h"

void setup() {
    Serial.begin(115200); // single Serial.begin — removed duplicate in App::init()
    App::init();
}

void loop() {
    App::run();
}