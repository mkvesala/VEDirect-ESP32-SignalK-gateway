#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class VEDSensor {
public:
    struct Snapshot {
        int32_t  mv  = INT32_MIN;   // millivolts, house battery
        int32_t  ma  = INT32_MIN;   // milliamps, house battery
        int32_t  w   = INT32_MIN;   // watts, house battery
        int32_t  soc = INT32_MIN;   // tenths of percent (955 = 95.5 %)
        int32_t  vs  = INT32_MIN;   // millivolts, starter battery
        uint32_t ts_mv  = 0;
        uint32_t ts_ma  = 0;
        uint32_t ts_w   = 0;
        uint32_t ts_soc = 0;
        uint32_t ts_vs  = 0;
    };

    VEDSensor();
    void begin();
    void getSnapshot(Snapshot& out) const;  // thread-safe via portMUX

private:
    static constexpr int      RX_PIN    = 16;
    static constexpr int      TX_PIN    = -1;
    static constexpr uint32_t VE_BAUD   = 19200;
    static constexpr size_t   LINE_SIZE = 64;
    static constexpr size_t   RX_BUF    = 4096;

    mutable portMUX_TYPE _mux;
    Snapshot             _cache;

    void cacheSet(volatile int32_t& slot, volatile uint32_t& ts, int32_t v);
    void runReaderLoop();

    static void readerTask(void* pvParameters);
};
