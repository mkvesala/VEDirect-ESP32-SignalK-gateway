#pragma once
#include <Arduino.h>
#include "VEDProcessor.h"

// Skeleton — NVS-tallennus toteutetaan myöhemmin
class VEDPreferences {
public:
    explicit VEDPreferences(VEDProcessor& processorRef);

    void load() {}
    bool loadWebPasswordHash(char* out_hash_65bytes) {
        out_hash_65bytes[0] = '\0';
        return false;
    }
    void saveWebPassword(const char* /*password_sha256_hex*/) {}

private:
    VEDProcessor& _processor;
    // static constexpr const char* NVS_NS = "ved";  // NVS namespace, käyttöön myöhemmin
};
