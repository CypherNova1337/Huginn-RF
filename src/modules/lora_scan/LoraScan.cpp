#include "LoraScan.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/Config.h"
#include "../../core/Logger.h"
#include "../../hal/Lora.h"

// name, freqMHz, bwKHz, sf, cr(4/x), syncWord
struct Preset { const char* name; float freq; float bw; uint8_t sf; uint8_t cr; uint8_t sync; };
static const Preset kPresets[] = {
    {"Mesh LongF", 906.875f, 250.0f, 11, 5, 0x2B},   // Meshtastic US LongFast
    {"Mesh EU",    869.525f, 250.0f, 11, 5, 0x2B},   // Meshtastic EU868
    {"915 SF7",    915.0f,   125.0f,  7, 5, 0x12},
    {"915 SF12",   915.0f,   125.0f, 12, 5, 0x12},
    {"868 SF7",    868.0f,   125.0f,  7, 5, 0x12},
};
static const int kNPresets = sizeof(kPresets) / sizeof(kPresets[0]);
static constexpr uint32_t kDwellMs = 2500;   // listen window per preset

void LoraScanModule::enter(Context& ctx) {
    view_ = View::Menu; menuSel_ = 0;
    meshFreq_ = ctx.config->getFloat("meshtastic.freq_mhz", 906.875f);
    ctx.display->clear(); ctx.display->header("LoRa Scan");
}

void LoraScanModule::exit(Context& ctx) {
    // Restore the configured default LoRa params for other modules.
    if (ctx.lora)
        ctx.lora->reconfigure(ctx.config->getFloat("radio.frequency_mhz", 915.0f),
                              ctx.config->getFloat("radio.bandwidth_khz", 125.0f),
                              (uint8_t)ctx.config->getInt("radio.spreading_factor", 7),
                              5, 0x12);
}

bool LoraScanModule::loop(Context& ctx) {
    switch (view_) {
        case View::Activity: return loopActivity(ctx);
        case View::Mesh:     return loopMesh(ctx);
        case View::Menu:     break;
    }
    static const char* items[] = {"Activity scan", "Meshtastic monitor"};
    ctx.display->list(items, 2, menuSel_, "LoRa Scan", "Enter=open  Esc=back");
    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   menuSel_ = (menuSel_ + 1) % 2; break;
        case Key::Down: menuSel_ = (menuSel_ + 1) % 2; break;
        case Key::Back: return false;
        case Key::Enter:
            if (!ctx.lora || !ctx.lora->ready()) break;
            if (menuSel_ == 0) {
                for (int i = 0; i < kNPresets; ++i) { hitCount_[i] = 0; bestRssi_[i] = -200; }
                presetIdx_ = 0; dwellStartMs_ = millis();
                const Preset& p = kPresets[0];
                ctx.lora->reconfigure(p.freq, p.bw, p.sf, p.cr, p.sync);
                view_ = View::Activity;
            } else {
                meshPkts_ = 0; meshRssi_ = -200; senderN_ = 0;
                ctx.lora->reconfigure(meshFreq_, 250.0f, 11, 5, 0x2B);
                view_ = View::Mesh;
            }
            break;
        default: break;
    }
    delay(10);
    return true;
}

// --- Activity scan ---------------------------------------------------------
bool LoraScanModule::loopActivity(Context& ctx) {
    Lora::RxPacket pkt;
    if (ctx.lora->receive(pkt)) {
        hitCount_[presetIdx_]++;
        if (pkt.rssi > bestRssi_[presetIdx_]) bestRssi_[presetIdx_] = pkt.rssi;
    }

    // Advance to the next preset when the dwell window elapses.
    if (millis() - dwellStartMs_ > kDwellMs) {
        presetIdx_ = (presetIdx_ + 1) % kNPresets;
        const Preset& p = kPresets[presetIdx_];
        ctx.lora->reconfigure(p.freq, p.bw, p.sf, p.cr, p.sync);
        dwellStartMs_ = millis();
    }

    if (millis() - lastUiMs_ > 250) {
        lastUiMs_ = millis();
        ctx.display->clear(); ctx.display->header("LoRa Activity");
        for (int i = 0; i < kNPresets; ++i) {
            bool cur = (i == presetIdx_);
            char buf[40];
            if (hitCount_[i])
                snprintf(buf, sizeof(buf), "%s%-10s %lu pk %.0fdBm", cur?">":" ",
                         kPresets[i].name, (unsigned long)hitCount_[i], bestRssi_[i]);
            else
                snprintf(buf, sizeof(buf), "%s%-10s --", cur?">":" ", kPresets[i].name);
            ctx.display->line(i, buf, cur ? 0xFFE0 : (hitCount_[i] ? 0x07E0 : 0x8410));
        }
        ctx.display->footer("sweeping presets  Esc=back");
    }
    if (ctx.input->poll().key == Key::Back) view_ = View::Menu;
    delay(2);
    return true;
}

// --- Meshtastic monitor ----------------------------------------------------
void LoraScanModule::noteSender(uint32_t id) {
    for (int i = 0; i < senderN_; ++i) if (senders_[i] == id) return;
    if (senderN_ < 32) senders_[senderN_++] = id;
}

bool LoraScanModule::loopMesh(Context& ctx) {
    Lora::RxPacket pkt;
    if (ctx.lora->receive(pkt)) {
        meshPkts_++;
        meshRssi_ = pkt.rssi;
        // Meshtastic PHY header: dest[0..3], sender[4..7] (little-endian node IDs).
        if (pkt.len >= 8) {
            uint32_t sender = pkt.data[4] | (pkt.data[5] << 8) |
                              (pkt.data[6] << 16) | ((uint32_t)pkt.data[7] << 24);
            noteSender(sender);
        }
        if (ctx.log) ctx.log->info("mesh pkt len=%u rssi=%.0f", (unsigned)pkt.len, pkt.rssi);
    }

    if (millis() - lastUiMs_ > 300) {
        lastUiMs_ = millis();
        ctx.display->clear(); ctx.display->header("Meshtastic mon");
        ctx.display->printf(0, 0xFFFF, "%.3f MHz  SF11/250", meshFreq_);
        ctx.display->printf(1, 0x07E0, "pkts:%lu  rssi:%.0f  nodes:%d",
            (unsigned long)meshPkts_, meshRssi_, senderN_);
        int rows = ctx.display->bodyRows() - 2;
        for (int i = 0; i < rows && i < senderN_; ++i) {
            char buf[24]; snprintf(buf, sizeof(buf), " !%08lx", (unsigned long)senders_[i]);
            ctx.display->line(2 + i, buf, 0xFFFF);
        }
        ctx.display->footer("listening  Esc=back");
    }
    if (ctx.input->poll().key == Key::Back) view_ = View::Menu;
    delay(2);
    return true;
}

REGISTER_MODULE(LoraScanModule);
