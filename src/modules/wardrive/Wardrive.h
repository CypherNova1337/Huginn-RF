// Wardrive.h — GPS-stamped wardriving across the radios this device has:
// WiFi APs, BLE devices, and (optionally) raw LoRa packets. Every sighting is
// de-duplicated and written to a WiGLE-ingestible CSV on the SD card. Entirely
// passive — it never transmits. Drive/walk around and it maps what's out there.
#pragma once
#include "../../core/Module.h"
#include <FS.h>
#include <map>

class WardriveModule : public Module {
public:
    const char* name() const override { return "Wardrive"; }
    Category    category() const override { return Category::Tools; }
    const char* description() const override { return "WiFi+BLE+LoRa -> WiGLE CSV (GPS)"; }
    bool        requiresAuthorization() const override { return false; } // passive

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;
    void exit(Context& ctx) override;

private:
    void writeHeader();
    void drawHud(Context& ctx, const char* status);
    bool fresh(const String& key, uint32_t now);
    void logRow(Context& ctx, const char* mac, const String& ssid,
                const String& auth, int rssi, int channel, const char* type);
    void scanWifi(Context& ctx);
    void scanBle(Context& ctx);
    void pollLora(Context& ctx);

    File     csv_;
    int      minRssi_ = -95;
    uint32_t dedupSec_ = 30;
    bool     doWifi_ = true, doBle_ = true, doLora_ = false;

    std::map<String, uint32_t> lastSeen_;   // key -> millis
    uint32_t wifiCount_ = 0, bleCount_ = 0, loraCount_ = 0;
    uint8_t  phase_ = 0;
    uint32_t lastUiMs_ = 0;
};
