// SubGhz.h — raw OOK/ASK sub-GHz capture, replay, and RSSI spectrum sweep via a
// CC1101 module (built with the cardputer_adv_cc1101 env; wire per HARDWARE.md).
// The SX1262 LoRa cap is specialised for chirp modulation and is a poor fit for
// arbitrary OOK remote cloning, so this module targets the CC1101. Without that
// hardware/build it shows a notice.
#pragma once
#include "../../core/Module.h"

class SubGhzModule : public Module {
public:
    const char* name() const override { return "Sub-GHz OOK"; }
    Category    category() const override { return Category::SubGHz; }
    const char* description() const override { return "CC1101 capture / replay / sweep"; }
    bool        requiresAuthorization() const override { return true; }

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;
    void exit(Context& ctx) override;

private:
    enum class View { Menu, Capture, Replay, Sweep };
    View view_ = View::Menu;
    int  menuSel_ = 0;

#ifdef HUGINN_HAS_CC1101
    static const size_t kMaxEdges = 2048;
    uint16_t timings_[kMaxEdges];     // mark/space durations in microseconds
    size_t   edgeCount_ = 0;
    float    freqMhz_ = 433.92f;
    bool     radioOk_ = false;

    bool initRadio(Context& ctx);
    bool loopCapture(Context& ctx);
    bool loopReplay(Context& ctx);
    bool loopSweep(Context& ctx);
#endif
    uint32_t lastUiMs_ = 0;
};
