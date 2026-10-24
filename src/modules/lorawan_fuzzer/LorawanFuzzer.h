// LorawanFuzzer.h — OTAA join-request parser fuzzer for backend robustness
// testing of YOUR OWN LoRaWAN network server (ChirpStack / private TTN).
//
// This is NOT a flooding tool. It exists to exercise the join-request parsing
// path of a server you operate, with malformed/boundary fields, so you can find
// crashes or mishandling. It is guarded three ways:
//   1. refuses to run unless config lorawan_fuzzer.target_is_mine == true
//      AND a non-empty, non-default `server` is set,
//   2. hard-caps the send rate (lorawan_fuzzer.max_requests_per_min),
//   3. every transmit is logged with a timestamp for your audit trail.
// Use only against infrastructure you own. See docs/LEGAL.md.
#pragma once
#include "../../core/Module.h"
#include <vector>

class LorawanFuzzerModule : public Module {
public:
    const char* name() const override { return "LoRaWAN Join Fuzzer"; }
    Category    category() const override { return Category::LoRa; }
    const char* description() const override { return "Join parser test (own server only)"; }
    bool        requiresAuthorization() const override { return true; }

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;
    void exit(Context& ctx) override;

private:
    enum class MutField { DevNonce, AppEui, DevEui, Mic };
    enum class Strategy { Random, Increment, Fixed, Boundary };
    struct Rule { MutField field; Strategy strat; };

    bool guardsPass(Context& ctx, String& why);
    void loadProfile(Context& ctx, const char* path);
    size_t buildJoinRequest(uint8_t* out);   // returns length
    void applyMutations(uint8_t* frame);

    std::vector<Rule> rules_;
    bool   armed_ = false;
    bool   blocked_ = true;
    String blockReason_;
    uint32_t sent_ = 0;
    uint16_t devNonce_ = 1;
    uint32_t windowStartMs_ = 0;
    uint32_t sentInWindow_ = 0;
    uint32_t maxPerMin_ = 30;
    uint32_t boundaryIdx_ = 0;
    uint32_t lastUiMs_ = 0;
};
