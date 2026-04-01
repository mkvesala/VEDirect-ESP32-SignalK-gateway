#include <Wire.h>
#include "VEDApplication.h"

VEDApplication app;

void setup() {
    Serial.begin(115200);
    delay(47);

    app.begin();
}

void loop() {
    app.loop();
}
