#include "ESPNowBroker.h"

ESPNowBroker::ESPNowBroker(VEDProcessor& processorRef)
    : _processor(processorRef)
{}

bool ESPNowBroker::begin() {
    if (esp_now_init() != ESP_OK) return false;

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST_ADDR, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) return false;

    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);

    _initialized = true;
    return true;
}

void ESPNowBroker::sendDelta() {
    if (!_initialized) return;

    ESPNow::ESPNowPacket<ESPNow::BatteryDelta> pkt;
    ESPNow::initHeader(pkt.hdr, ESPNow::ESPNowMsgType::BATTERY_DELTA,
                       sizeof(ESPNow::BatteryDelta));
    pkt.payload = _processor.getDelta();

    esp_now_send(BROADCAST_ADDR, reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
}

void ESPNowBroker::onDataSent(const esp_now_send_info_t* /*info*/,
                               esp_now_send_status_t /*status*/) {
    // Tyypillisesti tyhjä tai debug-logi
}

void ESPNowBroker::onDataRecv(const esp_now_recv_info_t* /*recv_info*/,
                               const uint8_t* data, int len) {
    // VED-gateway ei odota komentoja; validoi header ja ohita
    if (len < static_cast<int>(sizeof(ESPNow::ESPNowHeader))) return;
    ESPNow::ESPNowHeader hdr;
    memcpy(&hdr, data, sizeof(ESPNow::ESPNowHeader));
    if (hdr.magic != ESPNow::ESPNOW_MAGIC) return;
    // Tuntematon viestiTyyppi — ohitetaan
}
