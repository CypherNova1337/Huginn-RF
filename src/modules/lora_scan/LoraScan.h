// LoraScan.h — give the LoRa radio a real job.
//
//   Activity scan   cycle through common LoRa presets (incl. Meshtastic),
//                   listen briefly on each, and report where there's traffic
//                   and how strong — "is there LoRa here, and what kind?"
//   Meshtastic mon  retune to Meshtastic's radio params (SF11/BW250, sync
//                   0x2B) and listen: count packets and list the unique sender
//                   node IDs seen nearby — passive recon of who's meshing.
//
// Receive-only. Meshtastic payloads are encrypted; this reads the clear packet
// header (sender/dest node IDs), not message contents.
#pragma once
#include "../../core/Module.h"

class LoraScanModule : public Module {
public:
    const char* name() const override { return "LoRa Scan"; }
    Category    category() const override { return Category::LoRa; }
    const char* description() const override { return "Activity sweep + Meshtastic monitor"; }
    bool        requiresAuthorization() const override { return false; } // passive

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;
    void exit(Context& ctx) override;

private:
    enum class View { Menu, Activity, Mesh };
    View view_ = View::Menu;
    int  menuSel_ = 0;

    bool loopActivity(Context& ctx);
    bool loopMesh(Context& ctx);

    // Activity sweep state.
    int      presetIdx_ = 0;
    uint32_t hitCount_[8] = {0};
    float    bestRssi_[8];
    uint32_t dwellStartMs_ = 0;

    // Meshtastic monitor state.
    uint32_t meshPkts_ = 0;
    float    meshRssi_ = -200;
    uint32_t senders_[32];
    int      senderN_ = 0;
    float    meshFreq_ = 906.875f;
    uint32_t lastUiMs_ = 0;

    void noteSender(uint32_t id);
};
