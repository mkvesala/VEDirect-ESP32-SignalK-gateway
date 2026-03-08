#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "WifiState.h"
#include "VEDSensor.h"
#include "VEDProcessor.h"
#include "VEDPreferences.h"
#include "SignalKBroker.h"
#include "ESPNowBroker.h"
#include "DisplayManager.h"
#include "WebUIManager.h"

class VEDApplication {
public:
    explicit VEDApplication();

    void begin();
    void loop();

private:
    // Ajastusvakiot
    static constexpr unsigned long SENSOR_READ_MS       = 101;
    static constexpr unsigned long TX_INTERVAL_MS       = 1000;
    static constexpr unsigned long DISPLAY_INTERVAL_MS  = 1000;
    static constexpr unsigned long WIFI_STATUS_CHECK_MS = 503;
    static constexpr unsigned long WIFI_TIMEOUT_MS      = 90001;
    static constexpr unsigned long WS_RETRY_MS          = 1999;
    static constexpr unsigned long WS_RETRY_MAX_MS      = 119993;
    static constexpr unsigned long ESPNOW_TX_MS         = 1000;
    static constexpr uint8_t       NET_STATUS_EVERY     = 10;   // joka 10. display-tikki

    // Ajastimet
    unsigned long _last_read_ms       = 0;
    unsigned long _last_tx_ms         = 0;
    unsigned long _last_display_ms    = 0;
    unsigned long _last_espnow_tx_ms  = 0;
    unsigned long _wifi_conn_start_ms = 0;
    unsigned long _wifi_last_check_ms = 0;
    unsigned long _next_ws_try_ms     = 0;
    unsigned long _expn_retry_ms      = WS_RETRY_MS;
    uint8_t       _display_tick_ctr   = 0;

    WifiState _wifi_state = WifiState::INIT;

    // === LUOKAT — stack-allokoitu, elinkaari koko ohjelman ajan ===
    VEDSensor      _sensor;
    VEDProcessor   _processor;
    VEDPreferences _prefs;
    SignalKBroker  _signalk;
    ESPNowBroker   _espnow;
    DisplayManager _display;
    WebUIManager   _webui;

    // Loop-handlerit
    void handleWifi(unsigned long now);
    void handleOTA();
    void handleWebUI();
    void handleWebsocket(unsigned long now);
    void handleSensorRead(unsigned long now);
    void handleSignalK(unsigned long now);
    void handleESPNow(unsigned long now);
    void handleDisplay(unsigned long now);

    void initWifiServices();

    static const char* rssiLabel(int8_t rssi);
};
