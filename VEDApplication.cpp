#include "VEDApplication.h"
#include "secrets.h"

// === P U B L I C ===

// Constructor
VEDApplication::VEDApplication()
    : _sensor()
    , _processor(_sensor)
    , _prefs(_processor)
    , _signalk(_processor)
    , _espnow(_processor)
    , _display(_processor)
    , _webui(_processor, _prefs, _signalk, _display)
{}

// Initialize
void VEDApplication::begin() {
 
    // I2C
    Wire.begin(I2C_SDA, I2C_SCL);
    delay(47);

    // LCD
    _display.begin();
    _display.showMessage("STARTING...", "INIT WIFI...");

    // Not needed
    btStop();

    //  WiFi AP_STA — required for WiFi / ESP-NOW coexistence
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    _wifi_state = WifiState::CONNECTING;
    _wifi_conn_start_ms = millis();
    _display.showMessage("WIFI", "CONNECT...");

    // ESP-NOW
    _espnow.begin();

    // Sensor - runs on Core 0
    _sensor.begin();
}

// Loop
void VEDApplication::loop() {

    const unsigned long now = millis();

    this->handleWifi(now);
    this->handleOTA();
    this->handleWebUI();
    this->handleWebsocket(now);
    this->handleSensorRead(now);
    this->handleSignalK(now);
    this->handleESPNow(now);
    this->handleDisplay(now);

}

// === P R I V A T E ===

// WiFi state machine
void VEDApplication::handleWifi(unsigned long now) {

    if ((long)(now - _wifi_last_check_ms) < (long)WIFI_STATUS_CHECK_MS) return;
    _wifi_last_check_ms = now;

    switch (_wifi_state) {
        case WifiState::INIT:
            break;

        case WifiState::CONNECTING: {
            wl_status_t status = WiFi.status();
            if (status == WL_CONNECTED) {
                _wifi_state = WifiState::CONNECTED;
                this->initWifiServices();
                _expn_retry_ms = WS_RETRY_MS;
            } else if ((long)(now - _wifi_conn_start_ms) >= (long)WIFI_TIMEOUT_MS) {
                _wifi_state = WifiState::OFF;
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
                _display.showMessage("NO WIFI", "ESP-NOW ONLY");
            } else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
                _wifi_state = WifiState::OFF;
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
                _display.showMessage("WIFI FAILED", "ESP-NOW ONLY");
            }
            break;
        }

        case WifiState::CONNECTED: {
            if (!WiFi.isConnected()) {
                _wifi_state = WifiState::CONNECTING;
                WiFi.disconnect();
                WiFi.begin(WIFI_SSID, WIFI_PASS);
                _wifi_conn_start_ms = now;
            }
            break;
        }

        case WifiState::FAILED:
        case WifiState::DISCONNECTED:
        case WifiState::OFF:
            break;
    }
}

// WiFi dependent stuff
void VEDApplication::initWifiServices() {

    _signalk.begin();

    ArduinoOTA.setHostname(_signalk.getSignalKSource());
    ArduinoOTA.setPassword(OTA_PASS);
    ArduinoOTA.onStart([](){});
    ArduinoOTA.onEnd([](){});
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total){});
    ArduinoOTA.onError([](ota_error_t error){});
    ArduinoOTA.begin();

    _webui.begin();

    // Show IP address
    char ip_str[16];
    IPAddress ip = WiFi.localIP();
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    _display.showMessage("WIFI OK", ip_str);
    delay(2000);

    // Show RSSI
    char rssi_str[17];
    snprintf(rssi_str, sizeof(rssi_str), "SIG: %s", rssiLabel(WiFi.RSSI()));
    _display.showMessage("SIGNAL LEVEL:", rssi_str);
    delay(2000);

}

// OTA
void VEDApplication::handleOTA() {
    if (_wifi_state != WifiState::CONNECTED) return;
    ArduinoOTA.handle();
}

// WebUI
void VEDApplication::handleWebUI() {
    if (_wifi_state != WifiState::CONNECTED) return;
    _webui.handleRequest();
}

// Websocket
void VEDApplication::handleWebsocket(unsigned long now) {
    if (_wifi_state != WifiState::CONNECTED) return;
    _signalk.handleStatus();

    if (!_signalk.isOpen() && (long)(now - _next_ws_try_ms) >= 0) {
        _signalk.connectWebsocket();
        _next_ws_try_ms = now + _expn_retry_ms;
        _expn_retry_ms  = min(_expn_retry_ms * 2UL, WS_RETRY_MAX_MS);
    }
    if (_signalk.isOpen()) _expn_retry_ms = WS_RETRY_MS;
}

// Read sensor
void VEDApplication::handleSensorRead(unsigned long now) {
    if ((long)(now - _last_read_ms) < (long)SENSOR_READ_MS) return;
    _last_read_ms = now;
    _processor.update();
}

// Send to SignalK
void VEDApplication::handleSignalK(unsigned long now) {
    if (_wifi_state != WifiState::CONNECTED) return;
    if ((long)(now - _last_tx_ms) < (long)TX_INTERVAL_MS) return;
    _last_tx_ms = now;
    _signalk.sendDelta();
}

// ESP-NOW broadcast
void VEDApplication::handleESPNow(unsigned long now) {
    _espnow.processIncomingCommands();
    if ((long)(now - _last_espnow_tx_ms) < (long)ESPNOW_TX_MS) return;
    _last_espnow_tx_ms = now;
    _espnow.sendDelta();
}

// Display messages on LCD
void VEDApplication::handleDisplay(unsigned long now) {
    if ((long)(now - _last_display_ms) < (long)DISPLAY_INTERVAL_MS) return;
    _last_display_ms = now;

    const uint8_t t = ++_display_tick_ctr;
    const uint8_t phase = t % DISPLAY_CYCLE;

    if (phase == 0) {
        // WiFi + WebSocket
        if (_wifi_state == WifiState::OFF || _wifi_state == WifiState::FAILED) {
            _display.showMessage("NO WIFI", "ESP-NOW ONLY");
        } else if (_wifi_state == WifiState::CONNECTED) {
            char l1[17], l2[17];
            snprintf(l1, sizeof(l1), "WIFI %s", rssiLabel((int8_t)WiFi.RSSI()));
            snprintf(l2, sizeof(l2), "WS %s", _signalk.isOpen() ? "OPEN" : "CLOSED");
            _display.showMessage(l1, l2);
        } else {
            _display.showMessage("WIFI", "CONNECTING...");
        }
    } else if (phase == DISPLAY_CYCLE / 2) {
        // Diagnostics
        const uint32_t freeHeap   = ESP.getFreeHeap();
        const uint32_t mainWm     = (uint32_t)uxTaskGetStackHighWaterMark(NULL);
        const uint32_t readerWm   = _sensor.getReaderStackWatermark();
        _display.showDiagData(freeHeap, mainWm, readerWm);
    } else {
        _display.showBatteryData();
    }
}

// Helper for RSSI descriptor
const char* VEDApplication::rssiLabel(int8_t rssi) {
    if (rssi > -55) return "EXCELLENT";
    if (rssi < -80) return "POOR";
    return "OK";
}
