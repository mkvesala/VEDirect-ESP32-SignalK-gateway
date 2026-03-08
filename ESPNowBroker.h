#pragma once
#include <Arduino.h>
#include <esp_now.h>
#include "VEDProcessor.h"
#include "espnow_protocol.h"

class ESPNowBroker {
public:
    explicit ESPNowBroker(VEDProcessor& processorRef);

    bool begin();
    void sendDelta();
    void processIncomingCommands() {}  // stub — VED gateway ei vastaanota komentoja

private:
    static constexpr uint8_t BROADCAST_ADDR[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    VEDProcessor& _processor;
    bool          _initialized = false;

    static void onDataSent(const esp_now_send_info_t* info, esp_now_send_status_t status);
    static void onDataRecv(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len);
};
