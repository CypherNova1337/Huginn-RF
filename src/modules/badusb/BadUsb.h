// BadUsb.h — USB HID keystroke injection ("BadUSB"). Runs DuckyScript payloads
// stored on the SD card over the ESP32-S3's native USB as an emulated keyboard.
//
// Standard pentest-firmware feature: plug the device into a machine you are
// authorized to test and run a saved script. The DuckyScript parser is fully
// implemented; execution goes through the TinyUSB HID keyboard. Gated behind
// authorization, and every executed payload is logged.
#pragma once
#include "../../core/Module.h"
#include <vector>

class BadUsbModule : public Module {
public:
    const char* name() const override { return "BadUSB"; }
    Category    category() const override { return Category::USB; }
    const char* description() const override { return "Run DuckyScript HID payloads"; }
    bool        requiresAuthorization() const override { return true; }

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;
    void exit(Context& ctx) override;

private:
    void listPayloads(Context& ctx);
    void runPayload(Context& ctx, const String& path);
    // Execute a single DuckyScript line. `defaultDelay` persists across lines.
    void execLine(const String& line, uint32_t& defaultDelay);
    void pressCombo(const String& tokens);

    std::vector<String> files_;
    int  sel_ = 0;
    bool hidReady_ = false;
    bool running_ = false;
};
