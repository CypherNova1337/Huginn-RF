#include "Ir.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/SdCard.h"
#include "../../core/Config.h"
#include "../../core/Logger.h"
#include "../../hal/IrTx.h"
#include <FS.h>

void IrModule::enter(Context& ctx) {
    view_ = View::Menu; menuSel_ = 0;
    codes_.clear(); sel_ = 0; blasting_ = false; blastIdx_ = 0; protoSel_ = 0;
    String dir  = ctx.config->getString("ir.codes_dir", "/huginn/ir");
    String file = ctx.config->getString("ir.default_file", "tvbgone.ir");
    loadFile(ctx, (dir + "/" + file).c_str());
    ctx.display->clear(); ctx.display->header("IR Transmit");
}

void IrModule::loadFile(Context& ctx, const char* path) {
    if (!ctx.sd || !ctx.sd->exists(path)) return;
    File f = ctx.sd->fs().open(path, FILE_READ);
    if (!f) return;
    Code cur; bool have = false;
    while (f.available()) {
        String line = f.readStringUntil('\n'); line.trim();
        if (line.startsWith("#") || line.length() == 0) continue;
        if (line.startsWith("name:")) {
            if (have) codes_.push_back(cur);
            cur = Code(); cur.name = line.substring(5); cur.name.trim(); have = true;
        } else if (line.startsWith("frequency:")) {
            cur.freq = line.substring(10).toInt();
        } else if (line.startsWith("data:")) {
            String d = line.substring(5); d.trim();
            int start = 0;
            while (start < (int)d.length()) {
                int sp = d.indexOf(' ', start); if (sp < 0) sp = d.length();
                String tok = d.substring(start, sp);
                if (tok.length()) cur.timings.push_back((uint16_t)tok.toInt());
                start = sp + 1;
            }
        }
    }
    if (have) codes_.push_back(cur);
    f.close();
    if (ctx.log) ctx.log->info("IR: %u codes from %s", (unsigned)codes_.size(), path);
}

void IrModule::sendCode(Context& ctx, const Code& c) {
    if (c.timings.empty()) return;
    irtx::sendRaw(c.timings, c.freq);
    if (ctx.log) ctx.log->tx("ir", "sent '%s' (%u edges @%uHz)",
                             c.name.c_str(), (unsigned)c.timings.size(), c.freq);
}

bool IrModule::loop(Context& ctx) {
    switch (view_) {
        case View::File:  return loopFile(ctx);
        case View::Proto: return loopProto(ctx);
        case View::Menu:  break;
    }
    static const char* items[] = {"Send from file", "Send protocol code"};
    ctx.display->list(items, 2, menuSel_, "IR Transmit", "Enter=open  Esc=back");
    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   menuSel_ = (menuSel_ + 1) % 2; break;
        case Key::Down: menuSel_ = (menuSel_ + 1) % 2; break;
        case Key::Back: return false;
        case Key::Enter: view_ = (menuSel_ == 0) ? View::File : View::Proto; break;
        default: break;
    }
    delay(10);
    return true;
}

bool IrModule::loopFile(Context& ctx) {
    if (codes_.empty()) {
        ctx.display->clear(); ctx.display->header("IR: file");
        ctx.display->line(0, "No codes in /huginn/ir", 0xF800);
        ctx.display->footer("Esc=back");
        if (ctx.input->poll().key == Key::Back) view_ = View::Menu;
        delay(20); return true;
    }
    if (blasting_) {
        if (millis() - lastBlastMs_ > 120) {
            if (blastIdx_ < codes_.size()) { sendCode(ctx, codes_[blastIdx_++]); lastBlastMs_ = millis(); }
            else { blasting_ = false; blastIdx_ = 0; }
        }
        ctx.display->clear(); ctx.display->header("IR: file");
        ctx.display->printf(0, 0xFFE0, "Blasting %u/%u", (unsigned)blastIdx_, (unsigned)codes_.size());
        ctx.display->footer("Esc=stop");
        if (ctx.input->poll().key == Key::Back) { blasting_ = false; blastIdx_ = 0; }
        return true;
    }
    ctx.display->clear(); ctx.display->header("IR: file");
    ctx.display->printf(0, 0xFFFF, "%s", codes_[sel_].name.c_str());
    ctx.display->printf(1, 0x8410, "%d/%d  edges=%u", sel_+1, (int)codes_.size(),
                        (unsigned)codes_[sel_].timings.size());
    ctx.display->line(3, "Enter=send  B=blast all", 0x07E0);
    ctx.display->footer("Up/Dn  Esc=back");
    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   sel_ = (sel_ + codes_.size() - 1) % codes_.size(); break;
        case Key::Down: sel_ = (sel_ + 1) % codes_.size(); break;
        case Key::Enter: sendCode(ctx, codes_[sel_]); break;
        case Key::Back:  view_ = View::Menu; break;
        case Key::Char:  if (e.ch=='b'||e.ch=='B') { blasting_ = true; blastIdx_ = 0; } break;
        default: break;
    }
    delay(10);
    return true;
}

static long parseHex(const String& s) { return strtol(s.c_str(), nullptr, 16); }

bool IrModule::loopProto(Context& ctx) {
    static const char* protos[] = {"NEC", "NEC-ext", "Samsung", "Sony-12",
                                   "NEC brute (sweep cmd)"};
    const int N = 5;
    ctx.display->list(protos, N, protoSel_, "IR: protocol", "Enter=run  Esc=back");
    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   protoSel_ = (protoSel_ + N - 1) % N; break;
        case Key::Down: protoSel_ = (protoSel_ + 1) % N; break;
        case Key::Back: view_ = View::Menu; return true;
        case Key::Enter: {
            String a;
            if (!ctx.input->readLine("addr (hex):", a)) break;
            long addr = parseHex(a);

            if (protoSel_ == 4) {
                // Brute/guess: sweep every NEC command 0..255 for this address.
                if (ctx.log) ctx.log->tx("ir", "NEC brute addr=0x%lX 0..255", addr);
                for (int cmd = 0; cmd <= 255; ++cmd) {
                    irtx::sendRaw(irtx::encodeNEC(addr & 0xFF, cmd), 38000);
                    if ((cmd & 7) == 0) {
                        ctx.display->clear(); ctx.display->header("IR: NEC brute");
                        ctx.display->printf(0, 0xFFFF, "addr 0x%02lX", addr & 0xFF);
                        ctx.display->printf(1, 0xFFE0, "cmd 0x%02X  (%d/256)", cmd, cmd + 1);
                        ctx.display->footer("Esc=stop");
                        ctx.display->present();
                    }
                    if (ctx.input->poll().key == Key::Back) break;
                    delay(60);
                }
                break;
            }

            String c;
            if (!ctx.input->readLine("cmd (hex):", c)) break;
            long cmd = parseHex(c);
            std::vector<uint16_t> t; uint16_t freq = 38000;
            switch (protoSel_) {
                case 0: t = irtx::encodeNEC(addr & 0xFF, cmd & 0xFF); break;
                case 1: t = irtx::encodeNECext(addr & 0xFFFF, cmd & 0xFF); break;
                case 2: t = irtx::encodeSamsung(addr & 0xFF, cmd & 0xFF); break;
                case 3: t = irtx::encodeSony12(cmd & 0x7F, addr & 0x1F); freq = 40000; break;
            }
            irtx::sendRaw(t, freq);
            if (ctx.log) ctx.log->tx("ir", "%s addr=0x%lX cmd=0x%lX",
                                     protos[protoSel_], addr, cmd);
            ctx.display->clear(); ctx.display->header("IR: protocol");
            ctx.display->line(2, "Sent.", 0x07E0);
            delay(400);
            break;
        }
        default: break;
    }
    delay(10);
    return true;
}

void IrModule::exit(Context& ctx) {}

REGISTER_MODULE(IrModule);
