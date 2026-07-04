#include "SignalKBroker.h"
#include "secrets.h"
#include <esp_mac.h>

using namespace websockets;

// === P U B L I C ===

// Constructor
SignalKBroker::SignalKBroker(VEDProcessor& processorRef)
    : _processor(processorRef)
{}

// Initialize
bool SignalKBroker::begin() {
    setSignalKURL();
    setSignalKSource();
    return connectWebsocket();
}

// Keepalive
void SignalKBroker::handleStatus() {
    if (_ws_open) _ws.poll();
}

// Connect 
bool SignalKBroker::connectWebsocket() {
    _ws_open = _ws.connect(_sk_url);
    if (_ws_open) {
        _last_pong_ms = millis();   // seed liveness so a fresh socket is not flagged stale
        _ws.onMessage([this](WebsocketsMessage msg) {
            this->onMessage(msg);
        });
        _ws.onEvent([this](WebsocketsEvent event, const String& data) {
            this->onEvent(event, data);
        });
    }
    return _ws_open;
}

// Close
void SignalKBroker::closeWebsocket() {
    _ws.close();
    _ws_open = false;
    _last_pong_ms = 0;
}

// Send a client-initiated ping frame to probe liveness
void SignalKBroker::ping() {
    if (_ws_open) _ws.ping();
}

// Half-open detection: open, has been connected, but no pong within timeout
bool SignalKBroker::isStale(unsigned long now) const {
    return _ws_open && _last_pong_ms != 0 &&
           (long)(now - _last_pong_ms) >= (long)PONG_TIMEOUT_MS;
}

// Send data to SignalK
void SignalKBroker::sendDelta() {
    if (!_ws_open) return;

    ESPNow::BatteryDelta d = _processor.getDelta();

    _delta_doc.clear();
    _delta_doc["context"] = "vessels.self";
    auto updates = _delta_doc.createNestedArray("updates");
    auto up = updates.createNestedObject();
    up["$source"] = _sk_source;
    auto values = up.createNestedArray("values");

    auto add = [&](const char* path, float v) {
        if (!validf(v)) return;
        auto o = values.createNestedObject();
        o["path"] = path;
        o["value"] = v;
    };

    add("electrical.batteries.house.voltage", d.house_voltage);
    add("electrical.batteries.house.current", d.house_current);
    add("electrical.batteries.house.power", d.house_power);
    add("electrical.batteries.house.capacity.stateOfCharge", d.house_soc / 100.0f);
    add("electrical.batteries.start.voltage", d.start_voltage);

    if (values.size() == 0) return;

    char buf[JSON_BUF_SIZE];
    size_t n = serializeJson(_delta_doc, buf, sizeof(buf));
    if (!_ws.send(buf, n)) {
        _ws.close();
        _ws_open = false;
    }
}

// === P R I V A T E ===

// SignalK server URL
void SignalKBroker::setSignalKURL() {
    if (strlen(SK_TOKEN) > 0)
        snprintf(_sk_url, sizeof(_sk_url),
            "ws://%s:%u/signalk/v1/stream?token=%s", SK_HOST, SK_PORT, SK_TOKEN);
    else
        snprintf(_sk_url, sizeof(_sk_url),
            "ws://%s:%u/signalk/v1/stream", SK_HOST, SK_PORT);
}

// Source name for SignalK to identify sender
void SignalKBroker::setSignalKSource() {
    uint8_t m[6];
    esp_efuse_mac_get_default(m);
    snprintf(_sk_source, sizeof(_sk_source),
        "esp32.vedirect-%02x%02x%02x", m[3], m[4], m[5]);
}

// Callback for websocket
void SignalKBroker::onEvent(WebsocketsEvent event, const String& /*data*/) {
    switch (event) {
        case WebsocketsEvent::ConnectionOpened:
            _ws_open = true;
            _last_pong_ms = millis();   // seed liveness on open
            break;
        case WebsocketsEvent::ConnectionClosed:
            _ws_open = false;
            break;
        case WebsocketsEvent::GotPing:
            _ws.pong();
            break;
        case WebsocketsEvent::GotPong:
            _last_pong_ms = millis();   // liveness refresh — feeds isStale()
            break;
        default:
            break;
    }
}

// Callback - skeleton
void SignalKBroker::onMessage(WebsocketsMessage msg) {
    if (!msg.isText()) return;
    (void)msg;
}
