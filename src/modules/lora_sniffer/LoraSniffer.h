// LoraSniffer.h — passive raw LoRa packet sniffer. Listens on the configured
// frequency and logs every received packet (hex + RSSI/SNR + GPS) to the SD
// card. Receive-only; a good first step for reversing LoRa/LoRaWAN/Meshtastic
// traffic in an authorized setting.
#pragma once
#include "../../core/Module.h"
#include <FS.h>

class LoraSnifferModule : public Module {
public:
    const char* name() const override { return "LoRa Sniffer"; }
    Category    category() const override { return Category::LoRa; }
    const char* description() const override { return "Log raw LoRa packets (RX only)"; }
    bool        requiresAuthorization() const override { return false; } // passive

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;
    void exit(Context& ctx) override;

private:
    File     log_;
    uint32_t count_ = 0;
    uint32_t lastUiMs_ = 0;
    String   lastLine_;
};
