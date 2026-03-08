#include "VEDSensor.h"

// HardwareSerial instance on UART2
static HardwareSerial s_ved_serial(2);

VEDSensor::VEDSensor()
    : _mux(portMUX_INITIALIZER_UNLOCKED)
    , _cache()
{}

void VEDSensor::begin() {
    s_ved_serial.setRxBufferSize(RX_BUF);
    s_ved_serial.begin(VE_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);
    xTaskCreatePinnedToCore(readerTask, "vedread", 4096, this, 2, nullptr, 0);
}

void VEDSensor::getSnapshot(Snapshot& out) const {
    portENTER_CRITICAL(&_mux);
    out = _cache;
    portEXIT_CRITICAL(&_mux);
}

void VEDSensor::cacheSet(volatile int32_t& slot, volatile uint32_t& ts, int32_t v) {
    portENTER_CRITICAL(&_mux);
    slot = v;
    ts   = millis();
    portEXIT_CRITICAL(&_mux);
}

// ========= FreeRTOS task — Core 0 =========
void VEDSensor::readerTask(void* pvParameters) {
    static_cast<VEDSensor*>(pvParameters)->runReaderLoop();
}

void VEDSensor::runReaderLoop() {
    char   line[LINE_SIZE];
    size_t idx = 0;

    for (;;) {
        if (!s_ved_serial.available()) {
            vTaskDelay(1);
            continue;
        }

        int b = s_ved_serial.read();
        if (b < 0)    { vTaskDelay(1); continue; }
        if (b == '\r') continue;

        if (b == '\n') {
            line[idx] = '\0';
            idx = 0;

            char* save  = nullptr;
            char* label = strtok_r(line, "\t", &save);
            if (!label || !*label) { vTaskDelay(1); continue; }
            char* val   = strtok_r(nullptr, "\t", &save);
            if (!val   || !*val)   { vTaskDelay(1); continue; }

            long v;
            if (val[0] == 'O') v = (val[1] == 'N') ? 1 : 0;
            else                v = strtol(val, nullptr, 10);

            if      (strcmp(label, "V")   == 0) cacheSet(_cache.mv,  _cache.ts_mv,  (int32_t)v);
            else if (strcmp(label, "I")   == 0) cacheSet(_cache.ma,  _cache.ts_ma,  (int32_t)v);
            else if (strcmp(label, "P")   == 0) cacheSet(_cache.w,   _cache.ts_w,   (int32_t)v);
            else if (strcmp(label, "SOC") == 0) cacheSet(_cache.soc, _cache.ts_soc, (int32_t)v);
            else if (strcmp(label, "VS")  == 0) cacheSet(_cache.vs,  _cache.ts_vs,  (int32_t)v);

        } else {
            if (idx + 1 < LINE_SIZE) {
                line[idx++] = (char)b;
            } else {
                // overflow: flush rest of line
                while (s_ved_serial.available()) {
                    int bb = s_ved_serial.read();
                    if (bb == '\n' || bb < 0) break;
                }
                idx = 0;
            }
        }
    }
}
