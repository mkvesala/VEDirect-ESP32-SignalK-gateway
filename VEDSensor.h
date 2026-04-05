#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// === V E D S E N S O R  C L A S S ===
//
// - Class VEDSensor - "the sensor" responsible for communicating with VEDirect port
// - Initialise: _sensor.begin();
// - Read thread safe snapshot: _sensor.getSnapshot();
// - Owned by: VEDApplication

class VEDSensor {

public:

    struct Snapshot {
        int32_t  mv  = INT32_MIN;   // millivolts, house battery
        int32_t  ma  = INT32_MIN;   // milliamps, house battery
        int32_t  w   = INT32_MIN;   // watts, house battery
        int32_t  soc = INT32_MIN;   // tenths of percent (955 = 95.5 %)
        int32_t  vs  = INT32_MIN;   // millivolts, starter battery
        uint32_t ts_mv  = 0;        // timestamps vor values
        uint32_t ts_ma  = 0;
        uint32_t ts_w   = 0;
        uint32_t ts_soc = 0;
        uint32_t ts_vs  = 0;
    };

    explicit VEDSensor();

    void begin();
    void getSnapshot(Snapshot& out) const;
    uint32_t getReaderStackWatermark() const;

private:

    static constexpr int RX_PIN = 16;
    static constexpr int TX_PIN = -1;
    static constexpr uint32_t VE_BAUD = 19200;
    static constexpr size_t LINE_SIZE = 64;
    static constexpr size_t RX_BUF = 4096;

    mutable portMUX_TYPE _spinlock;
    Snapshot _cache;
    TaskHandle_t _reader_task_handle = nullptr;

    void cacheSet(volatile int32_t& slot, volatile uint32_t& ts, int32_t v);
    void runReaderLoop();

    static void readerTask(void* pvParameters);

};
