#include <Wire.h>
#include "VEDApplication.h"

VEDApplication app;

void setup() {
    Serial.begin(115200);
    delay(47);

    // I2C alustetaan ennen app.begin():iä jotta DisplayManager voi käyttää sitä
    Wire.begin(21, 22);
    delay(10);

    app.begin();
}

void loop() {
    app.loop();
}
