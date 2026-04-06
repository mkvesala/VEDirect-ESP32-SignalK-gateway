#pragma once

#include <Arduino.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include "VEDProcessor.h"

// === C L A S S  S I G N A L K B R O K E R ===
//
// - Class SignalKBroker - responsible for SignalK server communication
// - Init: _signalk.begin();
// - Provides public API to
//  - Keep websocket alive (poll)
//  - Connect and close websocket
//  - Get connection status and source name
//  - Send data to SignalK paths
// - Uses: VEDProcessor
// - Owns: WebsocketsClient
// - Owned by: VEDApplication

class SignalKBroker {

public:

    explicit SignalKBroker(VEDProcessor &processorRef);

    bool begin();
    void handleStatus();
    bool connectWebsocket();
    void closeWebsocket();
    void sendDelta();

    bool isOpen() const { return _ws_open; }
    const char* getSignalKSource() const { return _sk_source; }

private:

    static constexpr size_t SK_URL_SIZE    = 256;
    static constexpr size_t SK_SOURCE_SIZE = 32;
    static constexpr size_t JSON_BUF_SIZE  = 640;

    VEDProcessor &_processor;
    websockets::WebsocketsClient _ws;
    StaticJsonDocument<512> _delta_doc;
    StaticJsonDocument<256> _incoming_doc;

    bool _ws_open = false;
    char _sk_url[SK_URL_SIZE]{};
    char _sk_source[SK_SOURCE_SIZE]{};

    void setSignalKURL();
    void setSignalKSource();
    void onEvent(websockets::WebsocketsEvent event, const String& data);
    void onMessage(websockets::WebsocketsMessage msg);

};
