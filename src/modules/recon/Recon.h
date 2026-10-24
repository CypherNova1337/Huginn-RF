// Recon.h — live situational awareness for physical assessments.
//
//   Who's around   passively listens to 2.4 GHz WiFi activity (every device
//                   that transmits, including phones sending probe requests)
//                   and interleaves short BLE scans, maintaining a live table
//                   of nearby devices sorted by signal strength (= proximity).
//   Track device    lock onto one device (MAC) and watch a large RSSI meter so
//                   you can physically walk toward it ("getting warmer").
//
// Passive — never transmits. Honest limit: phones randomize their WiFi/BLE MAC,
// so this finds a *device in the moment*, not a named person over time. For
// authorized physical-security assessments only.
#pragma once
#include "../../core/Module.h"

class ReconModule : public Module {
public:
    const char* name() const override { return "Who's Around"; }
    Category    category() const override { return Category::Tools; }
    const char* description() const override { return "Live nearby devices + RSSI locate"; }
    bool        requiresAuthorization() const override { return false; } // passive

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;
    void exit(Context& ctx) override;

private:
    enum class View { List, Track };
    View view_ = View::List;

    void startCapture(Context& ctx);
    void stopCapture();
    void maybeBleScan(Context& ctx);
    bool loopList(Context& ctx);
    bool loopTrack(Context& ctx);

    int      sel_ = 0;
    uint8_t  target_[6] = {0};
    int8_t   trackPeak_ = -120;
    uint8_t  channel_ = 1;
    uint32_t forgetMs_ = 60000;
    uint32_t bleEveryMs_ = 6000;
    int      minRssi_ = -95;
    uint32_t lastHopMs_ = 0, lastBleMs_ = 0, lastUiMs_ = 0;
};
