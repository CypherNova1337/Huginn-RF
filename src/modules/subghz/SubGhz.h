// SubGhz.h — raw OOK/ASK sub-GHz capture & replay via a CC1101 module.
//
// The LoRa SX1262 cap is specialised for chirp (LoRa) modulation and is a poor
// fit for arbitrary OOK remote cloning, so this module targets a CC1101 wired
// per docs/HARDWARE.md and built with the `cardputer_adv_cc1101` env. Without
// that hardware it shows a notice. Capture/replay of raw timings is the core
// value; it is left as a hook to implement against the CC1101 driver.
#pragma once
#include "../../core/Module.h"

class SubGhzModule : public Module {
public:
    const char* name() const override { return "Sub-GHz OOK"; }
    Category    category() const override { return Category::SubGHz; }
    const char* description() const override { return "CC1101 capture/replay (OOK/ASK)"; }
    bool        requiresAuthorization() const override { return true; }

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;

private:
    void captureRaw(Context& ctx);   // TODO: CC1101 async RX -> timing buffer
    void replayRaw(Context& ctx);    // TODO: CC1101 async TX of timing buffer
};
