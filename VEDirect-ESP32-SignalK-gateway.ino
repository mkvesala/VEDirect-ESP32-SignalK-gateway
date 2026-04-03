#include <Arduino.h>
#include "VEDApplication.h"

// === M A I N  P R O G R A M ===
//
// - Owns VEDApplication instance
// - Initiates serial
// - Executes app.loop() in the main loop()

VEDApplication app;

void setup() {

    Serial.begin(115200);
    delay(47);

    app.begin();

}

void loop() {

    app.loop();

}
