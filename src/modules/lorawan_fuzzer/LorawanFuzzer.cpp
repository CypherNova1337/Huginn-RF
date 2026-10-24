#include "LorawanFuzzer.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/SdCard.h"
#include "../../core/Config.h"
#include "../../core/Logger.h"
#include "../../hal/Lora.h"
#include <FS.h>
#include <esp_random.h>

// LoRaWAN 1.0.x join-request PHYPayload layout:
//   MHDR(1) | AppEUI(8, little-endian) | DevEUI(8, LE) | DevNonce(2, LE) | MIC(4)
static constexpr size_t JR_LEN = 1 + 8 + 8 + 2 + 4;  // 23 bytes

void LorawanFuzzerModule::enter(Context& ctx) {
    ctx.display->clear();
    ctx.display->header("LoRaWAN Join Fuzzer");

    maxPerMin_ = ctx.config->getInt("lorawan_fuzzer.max_requests_per_min", 30);
    if (maxPerMin_ > 120) maxPerMin_ = 120;    // never allow a flood, even if misconfigured
    sent_ = 0; sentInWindow_ = 0; windowStartMs_ = millis();
    armed_ = false; boundaryIdx_ = 0; devNonce_ = 1;

    blocked_ = !guardsPass(ctx, blockReason_);

    String prof = ctx.config->getString("lorawan_fuzzer.profile",
                                        "/huginn/payloads/lorawan/join_basic.yaml");
    loadProfile(ctx, prof.c_str());
}

// --- Guards: own-server + non-default, else refuse to transmit. ---
bool LorawanFuzzerModule::guardsPass(Context& ctx, String& why) {
    if (!ctx.config->getBool("lorawan_fuzzer.target_is_mine", false)) {
        why = "target_is_mine != true"; return false;
    }
    String server = ctx.config->getString("lorawan_fuzzer.server", "");
    server.trim();
    if (server.length() == 0) { why = "server is empty"; return false; }
    if (server == "0.0.0.0" || server == "example.com") { why = "server is default"; return false; }
    return true;
}

void LorawanFuzzerModule::loadProfile(Context& ctx, const char* path) {
    rules_.clear();
    if (!ctx.sd || !ctx.sd->exists(path)) {
        // Default to a reasonable set if the profile file is missing.
        rules_ = {{MutField::DevNonce, Strategy::Random},
                  {MutField::AppEui,   Strategy::Boundary},
                  {MutField::DevEui,   Strategy::Boundary},
                  {MutField::Mic,      Strategy::Random}};
        return;
    }
    File f = ctx.sd->fs().open(path, FILE_READ);
    MutField curField = MutField::DevNonce; bool haveField = false;
    while (f && f.available()) {
        String line = f.readStringUntil('\n'); line.trim();
        if (line.startsWith("- field:")) {
            String v = line.substring(8); v.trim();
            haveField = true;
            if (v == "devnonce") curField = MutField::DevNonce;
            else if (v == "appeui") curField = MutField::AppEui;
            else if (v == "deveui") curField = MutField::DevEui;
            else if (v == "mic") curField = MutField::Mic;
            else haveField = false;
        } else if (line.startsWith("strategy:") && haveField) {
            String v = line.substring(9); v.trim();
            Strategy s = Strategy::Random;
            if (v == "increment") s = Strategy::Increment;
            else if (v == "fixed") s = Strategy::Fixed;
            else if (v == "boundary") s = Strategy::Boundary;
            rules_.push_back({curField, s});
            haveField = false;
        }
    }
    if (f) f.close();
    if (rules_.empty())
        rules_ = {{MutField::DevNonce, Strategy::Random}};
}

size_t LorawanFuzzerModule::buildJoinRequest(uint8_t* out) {
    memset(out, 0, JR_LEN);
    out[0] = 0x00;                          // MHDR: Join-Request, major 0
    // AppEUI + DevEUI default to zeros; DevNonce set; MIC left zero (mutated).
    out[1 + 8 + 8]     = devNonce_ & 0xFF;
    out[1 + 8 + 8 + 1] = (devNonce_ >> 8) & 0xFF;
    return JR_LEN;
}

static const uint8_t kBoundaryBytes[] = {0x00, 0xFF, 0x7F, 0x80, 0x01};

void LorawanFuzzerModule::applyMutations(uint8_t* frame) {
    for (const auto& r : rules_) {
        int off = 0, len = 0;
        switch (r.field) {
            case MutField::AppEui:   off = 1;      len = 8; break;
            case MutField::DevEui:   off = 1 + 8;  len = 8; break;
            case MutField::DevNonce: off = 1 + 16; len = 2; break;
            case MutField::Mic:      off = 1 + 18; len = 4; break;
        }
        switch (r.strat) {
            case Strategy::Random:
                esp_fill_random(frame + off, len);
                break;
            case Strategy::Increment:
                if (r.field == MutField::DevNonce) {
                    frame[off] = devNonce_ & 0xFF;
                    frame[off + 1] = (devNonce_ >> 8) & 0xFF;
                }
                break;
            case Strategy::Fixed:
                memset(frame + off, 0xA5, len);
                break;
            case Strategy::Boundary: {
                uint8_t b = kBoundaryBytes[boundaryIdx_ % sizeof(kBoundaryBytes)];
                memset(frame + off, b, len);
                break;
            }
        }
    }
    boundaryIdx_++;
    devNonce_++;
}

bool LorawanFuzzerModule::loop(Context& ctx) {
    // Re-draw status periodically.
    if (millis() - lastUiMs_ > 300) {
        lastUiMs_ = millis();
        ctx.display->clear();
        ctx.display->header("LoRaWAN Join Fuzzer");
        if (blocked_) {
            ctx.display->line(0, "BLOCKED — guard failed:", 0xF800);
            ctx.display->line(1, blockReason_.c_str(), 0xFFE0);
            ctx.display->line(3, "Set target_is_mine:true", 0x8410);
            ctx.display->line(4, "+ your server in config", 0x8410);
            ctx.display->footer("Esc=back");
        } else {
            ctx.display->printf(0, 0xFFFF, "Server: %s",
                ctx.config->getString("lorawan_fuzzer.server","").c_str());
            ctx.display->printf(1, 0x8410, "Rules:%u  Cap:%lu/min",
                (unsigned)rules_.size(), (unsigned long)maxPerMin_);
            ctx.display->printf(2, armed_ ? 0x07E0 : 0xFFE0,
                "State: %s", armed_ ? "ARMED" : "idle");
            ctx.display->printf(3, 0xFFFF, "Sent: %lu", (unsigned long)sent_);
            ctx.display->line(5, "Enter=arm/disarm", 0x07E0);
            ctx.display->footer("Esc=back");
        }
    }

    KeyEvent e = ctx.input->poll();
    if (e.key == Key::Back) return false;
    if (blocked_) { delay(20); return true; }
    if (e.key == Key::Enter) armed_ = !armed_;

    if (armed_) {
        // Enforce the per-minute cap with a sliding 60s window.
        uint32_t now = millis();
        if (now - windowStartMs_ >= 60000) { windowStartMs_ = now; sentInWindow_ = 0; }
        if (sentInWindow_ < maxPerMin_) {
            // Gentle inter-packet spacing derived from the cap.
            static uint32_t lastTx = 0;
            uint32_t minGap = 60000 / (maxPerMin_ ? maxPerMin_ : 1);
            if (now - lastTx >= minGap) {
                uint8_t frame[JR_LEN];
                buildJoinRequest(frame);
                applyMutations(frame);
                if (ctx.lora && ctx.lora->transmit(frame, JR_LEN)) {
                    sent_++; sentInWindow_++; lastTx = now;
                    if (ctx.log)
                        ctx.log->tx("lw-fuzzer", "join req #%lu (devnonce=%u)",
                                    (unsigned long)sent_, devNonce_);
                }
            }
        }
    }
    delay(2);
    return true;
}

void LorawanFuzzerModule::exit(Context& ctx) {
    armed_ = false;
    if (ctx.log) ctx.log->info("Join fuzzer stopped, %lu requests", (unsigned long)sent_);
}

REGISTER_MODULE(LorawanFuzzerModule);
