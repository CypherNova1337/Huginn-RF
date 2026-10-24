// Wardrive.h — passive sub-GHz/LoRa wardriving. Listens for packets on the
// configured frequency, stamps each with the current GPS fix, and logs to a
// WiGLE-ingestible CSV on the SD card. Receive-only; it never transmits.
#pragma once
#include "../../core/Module.h"
#include <FS.h>
#include <map>

class WardriveModule : public Module {
public:
    const char* name() const override { return "LoRa Wardrive"; }
    Category    category() const override { return Category::LoRa; }
    const char* description() const override { return "Map LoRa/sub-GHz pings -> WiGLE CSV"; }
    bool        requiresAuthorization() const override { return false; } // passive

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;
    void exit(Context& ctx) override;

private:
    uint32_t packetId(const uint8_t* data, size_t len) const;
    void     writeHeader(Context& ctx);

    File     csv_;
    uint32_t seen_ = 0;
    int      minRssi_ = -120;
    uint32_t dedupSec_ = 30;
    std::map<uint32_t, uint32_t> lastSeenMs_;  // id -> millis
    uint32_t lastUiMs_ = 0;
};
