#pragma once

#include <Arduino.h>
#include "VEDProcessor.h"
#include "VEDPreferences.h"
#include "SignalKBroker.h"
#include "DisplayManager.h"

// === C L A S S  W E B U I M A N A G E R ===
//
// - Class WebUIManager - responsible for running the webserver for http-requests
// - Not implemented in this version
// - Init: _webui.begin();
// - Loop: _webui.handleRequest();
// - Uses: VEDProcessor, VEDPreferences, SignalKBroker, DisplayManager
// - Owns: WebServer
// - Owned by: VEDApplication

class WebUIManager {

public:

    explicit WebUIManager(
        VEDProcessor &processorRef,
        VEDPreferences &prefsRef,
        SignalKBroker &signalkRef,
        DisplayManager &displayRef
    );

    void begin() {}
    void handleRequest() {}

private:

    VEDProcessor &_processor;
    VEDPreferences &_prefs;
    SignalKBroker &_signalk;
    DisplayManager &_display;

};
