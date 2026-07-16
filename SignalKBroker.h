#pragma once

#include <Arduino.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <memory>
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
// - Owns: WebsocketsClient (std::unique_ptr — fresh instance per reconnect)
// - Owned by: VEDApplication

class SignalKBroker {

public:

    explicit SignalKBroker(VEDProcessor &processorRef);

    bool begin();
    void handleStatus();
    bool connectWebsocket();
    void closeWebsocket();
    void sendDelta();
    void ping();
    bool isStale(unsigned long now) const;

    bool isOpen() const { return _ws_open; }
    const char* getSignalKSource() const { return _sk_source; }

private:

    static constexpr size_t SK_URL_SIZE    = 256;
    static constexpr size_t SK_SOURCE_SIZE = 32;
    static constexpr size_t JSON_BUF_SIZE  = 640;

    VEDProcessor &_processor;
    std::unique_ptr<websockets::WebsocketsClient> _ws;  // fresh instance every connect
    StaticJsonDocument<512> _delta_doc;
    StaticJsonDocument<256> _incoming_doc;

    // Liveness — half-open TCP detection via client ping / server pong
    static constexpr unsigned long PONG_TIMEOUT_MS = 29989UL;  // ~30 s w/o pong -> stale
    unsigned long _last_pong_ms = 0;   // millis() of last GotPong / open; 0 = not connected

    bool _ws_open = false;
    char _sk_url[SK_URL_SIZE]{};
    char _sk_source[SK_SOURCE_SIZE]{};

    void setSignalKURL();
    void setSignalKSource();
    void onEvent(websockets::WebsocketsEvent event, const String& data);
    void onMessage(websockets::WebsocketsMessage msg);

};
