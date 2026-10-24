#include "Ir.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/SdCard.h"
#include "../../core/Config.h"
#include "../../core/Logger.h"
#include "../../hal/board.h"
#include <FS.h>

// LEDC-based 38kHz-ish carrier gated by mark/space timings. Channel 0.
static constexpr int kLedcChannel = 0;

void IrModule::enter(Context& ctx) {
    codes_.clear();
    sel_ = 0; blasting_ = false; blastIdx_ = 0;

    String dir  = ctx.config->getString("ir.codes_dir", "/huginn/ir");
    String file = ctx.config->getString("ir.default_file", "tvbgone.ir");
    String path = dir + "/" + file;
    loadFile(ctx, path.c_str());

    // Configure LEDC on the IR pin; duty set per-mark at send time.
    ledcSetup(kLedcChannel, 38000, 8);
    ledcAttachPin(IR_TX_PIN, kLedcChannel);
    ledcWrite(kLedcChannel, 0);

    ctx.display->clear();
    ctx.display->header("IR Transmit");
}

void IrModule::loadFile(Context& ctx, const char* path) {
    if (!ctx.sd || !ctx.sd->exists(path)) return;
    File f = ctx.sd->fs().open(path, FILE_READ);
    if (!f) return;

    Code cur;
    bool have = false;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.startsWith("#") || line.length() == 0) continue;
        if (line.startsWith("name:")) {
            if (have) { codes_.push_back(cur); }
            cur = Code();
            cur.name = line.substring(5); cur.name.trim();
            have = true;
        } else if (line.startsWith("frequency:")) {
            cur.freq = line.substring(10).toInt();
        } else if (line.startsWith("data:")) {
            String d = line.substring(5); d.trim();
            int start = 0;
            while (start < (int)d.length()) {
                int sp = d.indexOf(' ', start);
                if (sp < 0) sp = d.length();
                String tok = d.substring(start, sp);
                if (tok.length()) cur.timings.push_back((uint16_t)tok.toInt());
                start = sp + 1;
            }
        }
    }
    if (have) codes_.push_back(cur);
    f.close();
    if (ctx.log) ctx.log->info("IR: loaded %u codes from %s",
                               (unsigned)codes_.size(), path);
}

void IrModule::sendRaw(const uint16_t* timings, size_t n, uint16_t freq) {
    ledcSetup(kLedcChannel, freq ? freq : 38000, 8);
    for (size_t i = 0; i < n; ++i) {
        bool mark = (i % 2 == 0);        // even = mark (carrier on)
        ledcWrite(kLedcChannel, mark ? 128 : 0);   // ~50% duty when on
        delayMicroseconds(timings[i]);
    }
    ledcWrite(kLedcChannel, 0);
}

void IrModule::sendCode(Context& ctx, const Code& c) {
    if (c.timings.empty()) return;
    sendRaw(c.timings.data(), c.timings.size(), c.freq);
    if (ctx.log) ctx.log->tx("ir", "sent '%s' (%u edges @%uHz)",
                             c.name.c_str(), (unsigned)c.timings.size(), c.freq);
}

bool IrModule::loop(Context& ctx) {
    if (codes_.empty()) {
        ctx.display->line(0, "No IR codes found.", 0xF800);
        ctx.display->line(1, "Add .ir files to /huginn/ir", 0x8410);
        ctx.display->footer("Esc=back");
        if (ctx.input->poll().key == Key::Back) return false;
        delay(20); return true;
    }

    // TV-B-Gone style blast: step through all codes with a gap between.
    if (blasting_) {
        if (millis() - lastBlastMs_ > 120) {
            if (blastIdx_ < codes_.size()) {
                sendCode(ctx, codes_[blastIdx_++]);
                lastBlastMs_ = millis();
            } else {
                blasting_ = false; blastIdx_ = 0;
            }
        }
        ctx.display->printf(0, 0xFFE0, "Blasting %u/%u",
                            (unsigned)blastIdx_, (unsigned)codes_.size());
        ctx.display->footer("Esc=stop");
        if (ctx.input->poll().key == Key::Back) { blasting_ = false; blastIdx_ = 0; }
        return true;
    }

    ctx.display->printf(0, 0xFFFF, "Code: %s", codes_[sel_].name.c_str());
    ctx.display->printf(1, 0x8410, "%d/%d  edges=%u",
                        sel_ + 1, (int)codes_.size(),
                        (unsigned)codes_[sel_].timings.size());
    ctx.display->line(3, "Enter=send  B=blast all", 0x07E0);
    ctx.display->footer("Up/Down select  Esc=back");

    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   sel_ = (sel_ + codes_.size() - 1) % codes_.size(); break;
        case Key::Down: sel_ = (sel_ + 1) % codes_.size(); break;
        case Key::Enter: sendCode(ctx, codes_[sel_]); break;
        case Key::Back:  return false;
        case Key::Char:
            if (e.ch == 'b' || e.ch == 'B') { blasting_ = true; blastIdx_ = 0; }
            break;
        default: break;
    }
    delay(10);
    return true;
}

void IrModule::exit(Context& ctx) {
    ledcWrite(kLedcChannel, 0);
    ledcDetachPin(IR_TX_PIN);
}

REGISTER_MODULE(IrModule);
