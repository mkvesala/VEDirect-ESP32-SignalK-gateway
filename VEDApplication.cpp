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

    //  WiFi AP_STA — required for WiFi / ESP-NOW coexistence.
    //  softAP() secures the AP interface immediately after mode() — before any
    //  client can connect. Hidden SSID, WPA2 password, max 1 connection.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS, 1 /*channel*/, 1 /*ssid_hidden*/, 1 /*max_connection*/);

    // 3rd line of AP defence — registered before WiFi.begin() so no event is missed.
    // Callback runs in the FreeRTOS "arduino_events" task: deauth immediately, flag loop().
    // MAC is copied before setting the flag so loop() always reads a complete address.
    WiFi.onEvent([this](arduino_event_id_t /*id*/, arduino_event_info_t info) {
        uint8_t aid = info.wifi_ap_staconnected.aid;
        memcpy(_ap_intruder_mac, info.wifi_ap_staconnected.mac, 6);
        esp_wifi_deauth_sta(aid);  // kick immediately — ESP-IDF call, thread-safe
        _ap_intruder = true;       // signal loop()
    }, ARDUINO_EVENT_WIFI_AP_STACONNECTED);

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

    const unsigned long loop_start = micros();
    const unsigned long now = millis();

    this->handleWifi(now);
    this->handleAPIntruder();
    this->handleOTA();
    this->handleWebUI();
    this->handleWebsocket(now);
    this->handleWatchdog(now);
    this->handleSensorRead(now);
    this->handleSignalK(now);
    this->handleESPNow(now);
    this->handleDisplay(now);

    this->monitorLoopRuntime(micros() - loop_start);

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

// WiFi dependent stuff — guarded so OTA and WebServer routes are registered exactly once
void VEDApplication::initWifiServices() {
    if (_wifi_services_initialized) return;
    _wifi_services_initialized = true;

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

// AP intruder alert — deauth already done in event callback; log + display here
void VEDApplication::handleAPIntruder() {
    if (!_ap_intruder) return;
    _ap_intruder = false;  // clear before Serial/display so a rapid second event isn't lost
    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             _ap_intruder_mac[0], _ap_intruder_mac[1], _ap_intruder_mac[2],
             _ap_intruder_mac[3], _ap_intruder_mac[4], _ap_intruder_mac[5]);
    Serial.printf("[AP] INTRUDER deauthed — MAC %s\n", mac);
    _display.showMessage("AP: INTRUDER!", mac);
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

    if (_signalk.isOpen()) {
        _last_ws_activity_ms = now;    // feed network watchdog
        _expn_retry_ms = WS_RETRY_MS;  // reset backoff only when already open
    } else {
        if ((long)(now - _next_ws_try_ms) >= 0) {
            _signalk.connectWebsocket();
            _next_ws_try_ms = now + _expn_retry_ms;
            _expn_retry_ms  = min(_expn_retry_ms * 2UL, WS_RETRY_MAX_MS);
        }
    }
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
    } else if (phase == DISPLAY_CYCLE / 4) {
        // Loop runtime + uptime
        char l1[17], l2[17];
        uint32_t avg_us = _monitoring ? (uint32_t)_loop_avg_us : 0;
        unsigned long s = millis() / 1000;
        snprintf(l1, sizeof(l1), "LOOP %lu us", avg_us);
        snprintf(l2, sizeof(l2), "UP %lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
        _display.showMessage(l1, l2);
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

// Watchdog — restart on bloated loop runtime, or if WiFi up but TCP/IP stack silently dead
void VEDApplication::handleWatchdog(unsigned long now) {
    if (_monitoring && _loop_avg_us > LOOP_WATCHDOG_US) {
        _display.showMessage("LOOP WATCHDOG", "RESTARTING...");
        delay(1999);
        ESP.restart();
    }
    if (_wifi_state != WifiState::CONNECTED) return;
    if (_signalk.isOpen()) return;
    if (_last_ws_activity_ms == 0) return;
    if ((long)(now - _last_ws_activity_ms) < (long)WS_WATCHDOG_MS) return;
    _display.showMessage("WATCHDOG", "RESTARTING...");
    delay(1999);
    ESP.restart();
}

// EMA loop runtime monitor — alpha=0.01, initialised on first call
// Called at END of loop() so watchdog sees previous iteration's EMA, not current
void VEDApplication::monitorLoopRuntime(unsigned long us) {
    if (!_monitoring) {
        _loop_avg_us = us;
        _monitoring = true;
    } else {
        _loop_avg_us = 0.01f * us + 0.99f * _loop_avg_us;
    }
}

// Helper for RSSI descriptor
const char* VEDApplication::rssiLabel(int8_t rssi) {
    if (rssi > -55) return "EXCELLENT";
    if (rssi < -80) return "POOR";
    return "OK";
}
