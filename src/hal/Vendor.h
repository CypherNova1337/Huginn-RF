// Vendor.h — best-effort device identification. Turns a MAC OUI (first 3 bytes)
// or a BLE manufacturer company-ID into a human vendor name, so the recon tools
// say "Apple / Samsung / Espressif / Ring" instead of raw hex. A curated subset
// of common consumer vendors — enough to make a list readable, not exhaustive.
#pragma once
#include <Arduino.h>

namespace vendor {
    // mac points to >=3 bytes. Returns a short vendor name, or "" if unknown.
    const char* forOui(const uint8_t* mac);
    // BLE "Company Identifier" (little-endian in advertisement mfg data).
    const char* forCompanyId(uint16_t id);
    // True if the MAC looks locally-administered / randomized (bit 1 of byte 0).
    inline bool isRandom(const uint8_t* mac) { return (mac[0] & 0x02) != 0; }
}
