#pragma once
#include <Arduino.h>
#include "VEDProcessor.h"
#include "VEDPreferences.h"
#include "SignalKBroker.h"
#include "DisplayManager.h"

// Skeleton — Web-käyttöliittymä toteutetaan myöhemmin
class WebUIManager {
public:
    explicit WebUIManager(
        VEDProcessor&   processorRef,
        VEDPreferences& prefsRef,
        SignalKBroker&  signalkRef,
        DisplayManager& displayRef
    );

    void begin() {}
    void handleRequest() {}

private:
    VEDProcessor&   _processor;
    VEDPreferences& _prefs;
    SignalKBroker&  _signalk;
    DisplayManager& _display;
};
