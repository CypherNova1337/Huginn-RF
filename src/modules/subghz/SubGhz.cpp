#include "SubGhz.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/Logger.h"
#include "../../hal/board.h"

#ifdef HUGINN_HAS_CC1101
#include <RadioLib.h>
#include <SPI.h>

static SPIClass s_ccSpi(FSPI);
static CC1101 s_cc = new Module(CC1101_PIN_CS, CC1101_PIN_GDO0,
                                RADIOLIB_NC, CC1101_PIN_GDO2, s_ccSpi);

bool SubGhzModule::initRadio(Context& ctx) {
    s_ccSpi.begin(CC1101_PIN_SCK, CC1101_PIN_MISO, CC1101_PIN_MOSI, CC1101_PIN_CS);
    int st = s_cc.begin(freqMhz_);
    if (st != RADIOLIB_ERR_NONE) { radioOk_ = false; return false; }
    s_cc.setOOK(true);
    s_cc.setCrcFiltering(false);
    s_cc.setEncoding(RADIOLIB_ENCODING_NRZ);
    radioOk_ = true;
    return true;
}

// Capture raw OOK by sampling the GDO0 data line for edge timings.
bool SubGhzModule::loopCapture(Context& ctx) {
    ctx.display->clear(); ctx.display->header("Sub-GHz Capture");
    ctx.display->printf(0, 0xFFFF, "Freq: %.2f MHz", freqMhz_);
    ctx.display->line(1, "Listening for a burst...", 0xFFE0);
    ctx.display->footer("Esc=back");

    s_cc.receiveDirect();
    pinMode(CC1101_PIN_GDO0, INPUT);
    edgeCount_ = 0;

    // Wait for activity, then record edge-to-edge durations until a long gap.
    uint32_t idleStart = micros();
    int last = digitalRead(CC1101_PIN_GDO0);
    uint32_t lastEdge = micros();
    bool started = false;

    while (edgeCount_ < kMaxEdges) {
        if (ctx.input->poll().key == Key::Back) break;
        int cur = digitalRead(CC1101_PIN_GDO0);
        uint32_t now = micros();
        if (cur != last) {
            uint32_t dur = now - lastEdge;
            if (!started) { started = true; }
            else if (dur < 65535) timings_[edgeCount_++] = (uint16_t)dur;
            lastEdge = now; last = cur;
        }
        // End of transmission: long idle after we started capturing.
        if (started && (now - lastEdge) > 20000) break;
        if (!started && (now - idleStart) > 8000000UL) break;  // 8s no-signal timeout
    }

    if (edgeCount_ > 4 && ctx.log)
        ctx.log->info("SubGHz captured %u edges @%.2fMHz", (unsigned)edgeCount_, freqMhz_);
    view_ = View::Menu;
    return true;
}

bool SubGhzModule::loopReplay(Context& ctx) {
    ctx.display->clear(); ctx.display->header("Sub-GHz Replay");
    if (edgeCount_ < 4) {
        ctx.display->line(0, "Nothing captured yet.", 0xF800);
        ctx.display->footer("Esc=back");
        if (ctx.input->poll().key == Key::Back) view_ = View::Menu;
        delay(20); return true;
    }
    ctx.display->printf(0, 0xFFFF, "Replaying %u edges", (unsigned)edgeCount_);
    ctx.display->printf(1, 0x8410, "@ %.2f MHz", freqMhz_);
    ctx.display->footer("sending...");

    s_cc.transmitDirect();
    pinMode(CC1101_PIN_GDO0, OUTPUT);
    int level = 1;                        // first recorded duration is a mark
    for (size_t i = 0; i < edgeCount_; ++i) {
        digitalWrite(CC1101_PIN_GDO0, level);
        delayMicroseconds(timings_[i]);
        level ^= 1;
    }
    digitalWrite(CC1101_PIN_GDO0, 0);
    s_cc.standby();

    if (ctx.log) ctx.log->tx("subghz", "replayed %u edges @%.2fMHz",
                             (unsigned)edgeCount_, freqMhz_);
    view_ = View::Menu;
    return true;
}

bool SubGhzModule::loopSweep(Context& ctx) {
    // Coarse RSSI sweep across a band to spot active channels.
    static float f = 300.0f;
    if (f > 928.0f) f = 300.0f;
    s_cc.setFrequency(f);
    delay(3);
    float rssi = s_cc.getRSSI();

    if (millis() - lastUiMs_ > 60) {
        lastUiMs_ = millis();
        ctx.display->printf(0, 0xFFFF, "Sweep %.1f MHz", f);
        ctx.display->printf(1, rssi > -90 ? 0x07E0 : 0x8410, "RSSI %.0f dBm", rssi);
        ctx.display->footer("Esc=back");
    }
    f += 0.5f;
    if (ctx.input->poll().key == Key::Back) { s_cc.setFrequency(freqMhz_); view_ = View::Menu; }
    return true;
}
#endif // HUGINN_HAS_CC1101

void SubGhzModule::enter(Context& ctx) {
    view_ = View::Menu; menuSel_ = 0;
    ctx.display->clear(); ctx.display->header("Sub-GHz OOK");
#ifdef HUGINN_HAS_CC1101
    initRadio(ctx);
#endif
}

bool SubGhzModule::loop(Context& ctx) {
#ifndef HUGINN_HAS_CC1101
    ctx.display->clear(); ctx.display->header("Sub-GHz OOK");
    ctx.display->line(0, "No CC1101 in this build.", 0xF800);
    ctx.display->line(1, "SX1262 LoRa cap can't do", 0xFFFF);
    ctx.display->line(2, "general OOK cloning well.", 0xFFFF);
    ctx.display->line(3, "Flash cardputer_adv_cc1101", 0x8410);
    ctx.display->line(4, "with a CC1101 attached.", 0x8410);
    ctx.display->footer("Esc=back");
    if (ctx.input->poll().key == Key::Back) return false;
    delay(20);
    return true;
#else
    switch (view_) {
        case View::Capture: return loopCapture(ctx);
        case View::Replay:  return loopReplay(ctx);
        case View::Sweep:   return loopSweep(ctx);
        case View::Menu:    break;
    }
    if (!radioOk_) {
        ctx.display->clear(); ctx.display->header("Sub-GHz OOK");
        ctx.display->line(0, "CC1101 not found.", 0xF800);
        ctx.display->line(1, "Check wiring (board.h).", 0x8410);
        ctx.display->footer("Esc=back");
        if (ctx.input->poll().key == Key::Back) return false;
        delay(20); return true;
    }
    static const char* items[] = {"Capture (OOK)", "Replay last", "RSSI sweep"};
    ctx.display->list(items, 3, menuSel_, "Sub-GHz OOK", "Enter=open  Esc=back");
    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   menuSel_ = (menuSel_ + 2) % 3; break;
        case Key::Down: menuSel_ = (menuSel_ + 1) % 3; break;
        case Key::Back: return false;
        case Key::Enter:
            view_ = (menuSel_ == 0) ? View::Capture
                  : (menuSel_ == 1) ? View::Replay : View::Sweep;
            break;
        default: break;
    }
    delay(10);
    return true;
#endif
}

void SubGhzModule::exit(Context& ctx) {
#ifdef HUGINN_HAS_CC1101
    if (radioOk_) s_cc.standby();
#endif
}

REGISTER_MODULE(SubGhzModule);
