#include "VEDProcessor.h"
#include <cmath>

VEDProcessor::VEDProcessor(VEDSensor& sensorRef)
    : _sensor(sensorRef)
    , _delta{ NAN, NAN, NAN, NAN, NAN }
{}

void VEDProcessor::update() {
    VEDSensor::Snapshot snap;
    _sensor.getSnapshot(snap);

    _delta.house_voltage = ageOk(snap.ts_mv, STALE_MS)
        ? mvToV(snap.mv) : NAN;

    _delta.house_current = ageOk(snap.ts_ma, STALE_MS)
        ? maToA(snap.ma) : NAN;

    _delta.house_power   = ageOk(snap.ts_w, STALE_MS)
        ? static_cast<float>(snap.w) : NAN;

    _delta.house_soc     = ageOk(snap.ts_soc, STALE_MS)
        ? socToPercent(snap.soc) : NAN;

    _delta.start_voltage = ageOk(snap.ts_vs, STALE_MS)
        ? mvToV(snap.vs) : NAN;
}

bool VEDProcessor::hasValidData() const {
    return validf(_delta.house_voltage)
        || validf(_delta.house_current)
        || validf(_delta.house_power)
        || validf(_delta.house_soc)
        || validf(_delta.start_voltage);
}
