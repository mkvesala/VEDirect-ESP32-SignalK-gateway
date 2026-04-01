#pragma once
#include <Arduino.h>
#include "VEDSensor.h"
#include "espnow_protocol.h"

// Float-validaattori — käytetään kaikkialla
inline bool validf(float x) { return !isnan(x) && isfinite(x); }

class VEDProcessor {
public:
    explicit VEDProcessor(VEDSensor& sensorRef);

    void update();  // kutsutaan handleSensorRead():ssa ~100 ms välein

    // Delta SignalKBrokerille ja ESPNowBrokerille
    ESPNow::BatteryDelta getDelta() const { return _delta; }

    // Getterit DisplayManagerille
    float getHouseVoltage() const { return _delta.house_voltage; }
    float getHouseCurrent() const { return _delta.house_current; }
    float getHousePower()   const { return _delta.house_power;   }
    float getHouseSoc()     const { return _delta.house_soc;     }
    float getStartVoltage() const { return _delta.start_voltage; }

    bool hasValidData() const;

private:
    static constexpr uint32_t STALE_MS = 30000;

    VEDSensor&           _sensor;
    ESPNow::BatteryDelta _delta;  // jäsenalustus .cpp konstruktorissa NAN

    static float mvToV(int32_t mv)       { return mv / 1000.0f; }
    static float maToA(int32_t ma)       { return ma / 1000.0f; }
    static float socToPercent(int32_t raw) {
        float x = raw / 10.0f;
        if (x <   0.0f) x =   0.0f;
        if (x > 100.0f) x = 100.0f;
        return x;
    }
    static bool ageOk(uint32_t ts, uint32_t max_ms) {
        return (ts != 0) && ((millis() - ts) <= max_ms);
    }
};
