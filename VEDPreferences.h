#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "VEDProcessor.h"

// === C L A S S  V E D P R E F E R E N C E S ===
//
// - Class VEDPreferences - responsible for NVS persistence
// - Currently obsolete skeleton
// - Uses: VEDProcessor
// - Owned by: VEDApplication

class VEDPreferences {

public:

    explicit VEDPreferences(VEDProcessor& processorRef);

    void load() {}
    void save() {}

private:

    VEDProcessor& _processor;
    static constexpr const char* NVS_NS = "ved";
    
};
