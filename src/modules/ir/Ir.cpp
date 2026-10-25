#include "Ir.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/SdCard.h"
#include "../../core/Config.h"
#include "../../core/Logger.h"
#include "../../hal/IrTx.h"
#include <FS.h>

static long parseHex(const String& s) { return strtol(s.c_str(), nullptr, 16); }

// --- Built-in common power codes (best-effort; use the library for exact) ---
enum IrProto { P_NEC, P_NECX, P_SAMSUNG, P_SONY12, P_SONY20, P_RC5 };
struct PowerCode { IrProto proto; uint32_t addr; uint8_t cmd; const char* name; };
static const PowerCode kPower[] = {
    {P_SAMSUNG, 0x07,   0x02, "Samsung"},
    {P_NEC,     0x04,   0x08, "LG"},
    {P_SONY12,  0x01,   0x15, "Sony"},
    {P_RC5,     0x00,   0x0C, "Philips"},
    {P_NEC,     0x00,   0x17, "Vizio"},
    {P_NEC,     0x08,   0x05, "TCL/Roku"},
    {P_NECX,  0x20DF,   0x10, "LG(ext)"},
    {P_NEC,     0x40,   0x90, "Panason."},
    {P_NEC,     0x02,   0x5D, "Insignia"},
    {P_NEC,     0x87,   0x5F, "Hisense"},
};
static const int kNPower = sizeof(kPower) / sizeof(kPower[0]);

static std::vector<uint16_t> encodeFor(IrProto p, uint32_t addr, uint8_t cmd, uint16_t& freq) {
    freq = 38000;
    switch (p) {
        case P_NEC:     return irtx::encodeNEC(addr & 0xFF, cmd);
        case P_NECX:    return irtx::encodeNECext(addr & 0xFFFF, cmd);
        case P_SAMSUNG: return irtx::encodeSamsung(addr & 0xFF, cmd);
        case P_SONY12:  freq = 40000; return irtx::encodeSony12(cmd & 0x7F, addr & 0x1F);
        case P_SONY20:  freq = 40000; return irtx::encodeSony20(cmd & 0x7F, addr & 0x1FFF);
        case P_RC5:     freq = 36000; return irtx::encodeRC5(addr & 0x1F, cmd & 0x3F);
    }
    return {};
}

void IrModule::enter(Context& ctx) {
    view_ = View::Menu; menuSel_ = 0; protoSel_ = 0;
    blasting_ = false; blastIdx_ = 0;
    ctx.display->clear(); ctx.display->header("IR Transmit");
}
void IrModule::exit(Context& ctx) {}

// --- Library (file browser) ------------------------------------------------
void IrModule::loadLibrary(Context& ctx) {
    libFiles_.clear(); libSel_ = 0;
    String dir = ctx.config->getString("ir.codes_dir", "/huginn/ir");
    if (!ctx.sd || !ctx.sd->exists(dir.c_str())) return;
    File d = ctx.sd->fs().open(dir.c_str());
    for (File e = d.openNextFile(); e; e = d.openNextFile()) {
        if (!e.isDirectory()) {
            String n = e.name(); int sl = n.lastIndexOf('/'); if (sl >= 0) n = n.substring(sl + 1);
            if (n.endsWith(".ir")) libFiles_.push_back(dir + "/" + n);
        }
        e.close();
    }
    if (d) d.close();
}

void IrModule::loadFile(Context& ctx, const char* path) {
    codes_.clear(); sel_ = 0; blasting_ = false;
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
}

void IrModule::sendCode(Context& ctx, const Code& c) {
    if (c.timings.empty()) return;
    irtx::sendRaw(c.timings, c.freq);
    last_ = c;                                   // remember for "save"
    if (ctx.log) ctx.log->tx("ir", "sent '%s' (%u edges @%uHz)",
                             c.name.c_str(), (unsigned)c.timings.size(), c.freq);
}

void IrModule::smartPowerBlast(Context& ctx) {
    if (ctx.log) ctx.log->tx("ir", "smart power blast (%d codes)", kNPower);
    for (int i = 0; i < kNPower; ++i) {
        uint16_t freq; auto t = encodeFor(kPower[i].proto, kPower[i].addr, kPower[i].cmd, freq);
        if (!t.empty()) irtx::sendRaw(t, freq);
        ctx.display->clear(); ctx.display->header("IR smart power");
        ctx.display->printf(0, 0xFFFF, "%d/%d", i + 1, kNPower);
        ctx.display->printf(1, 0xFFE0, "%s", kPower[i].name);
        ctx.display->footer("Esc=stop");
        ctx.display->present();
        if (ctx.input->poll().key == Key::Back) break;
        delay(150);
    }
}

void IrModule::saveLast(Context& ctx) {
    if (last_.timings.empty()) {
        ctx.display->clear(); ctx.display->header("IR save");
        ctx.display->line(0, "No code sent yet.", 0xF800);
        ctx.display->footer("any key"); ctx.display->present();
        while (ctx.input->poll().key == Key::None) delay(10);
        return;
    }
    String dir = ctx.config->getString("ir.codes_dir", "/huginn/ir");
    String fname, cname;
    if (!ctx.input->readLine("file (my_remote):", fname) || !fname.length()) return;
    if (!fname.endsWith(".ir")) fname += ".ir";
    if (!ctx.input->readLine("button name:", cname) || !cname.length()) cname = "button";
    ctx.sd->mkdirs(dir.c_str());
    String path = dir + "/" + fname;
    File f = ctx.sd->fs().open(path.c_str(), FILE_APPEND);
    if (!f) return;
    f.printf("#\nname: %s\ntype: raw\nfrequency: %u\nduty_cycle: 0.33\ndata:",
             cname.c_str(), last_.freq);
    for (auto t : last_.timings) f.printf(" %u", t);
    f.println();
    f.close();
    if (ctx.log) ctx.log->info("IR saved '%s' -> %s", cname.c_str(), path.c_str());
}

bool IrModule::loop(Context& ctx) {
    switch (view_) {
        case View::Library:   return loopLibrary(ctx);
        case View::FileCodes: return loopFileCodes(ctx);
        case View::Proto:     return loopProto(ctx);
        case View::Menu:      break;
    }
    static const char* items[] = {"Browse library", "Send protocol",
                                  "Smart power blast", "Save last code"};
    const int N = 4;
    ctx.display->list(items, N, menuSel_, "IR Transmit", "Enter=open  Esc=back");
    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   menuSel_ = (menuSel_ + N - 1) % N; break;
        case Key::Down: menuSel_ = (menuSel_ + 1) % N; break;
        case Key::Back: return false;
        case Key::Enter:
            if (menuSel_ == 0)      { loadLibrary(ctx); view_ = View::Library; }
            else if (menuSel_ == 1) { view_ = View::Proto; }
            else if (menuSel_ == 2) { smartPowerBlast(ctx); }
            else                    { saveLast(ctx); }
            break;
        default: break;
    }
    delay(10);
    return true;
}

bool IrModule::loopLibrary(Context& ctx) {
    if (libFiles_.empty()) {
        ctx.display->clear(); ctx.display->header("IR library");
        ctx.display->line(0, "No .ir files in", 0xF800);
        ctx.display->line(1, "/huginn/ir", 0x8410);
        ctx.display->footer("Esc=back");
        if (ctx.input->poll().key == Key::Back) view_ = View::Menu;
        delay(20); return true;
    }
    ctx.display->clear(); ctx.display->header("IR library");
    int rows = ctx.display->bodyRows();
    int first = libSel_ >= rows ? libSel_ - rows + 1 : 0;
    for (int r = 0; r < rows && (first + r) < (int)libFiles_.size(); ++r) {
        int i = first + r; bool s = (i == libSel_);
        String n = libFiles_[i]; int sl = n.lastIndexOf('/'); if (sl >= 0) n = n.substring(sl + 1);
        ctx.display->line(r, ((s ? "> " : "  ") + n).c_str(), s ? 0xFFE0 : 0xFFFF);
    }
    ctx.display->footer("Up/Dn  Enter=open  Esc=back");
    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   libSel_ = (libSel_ + libFiles_.size() - 1) % libFiles_.size(); break;
        case Key::Down: libSel_ = (libSel_ + 1) % libFiles_.size(); break;
        case Key::Enter: loadFile(ctx, libFiles_[libSel_].c_str()); view_ = View::FileCodes; break;
        case Key::Back: view_ = View::Menu; break;
        default: break;
    }
    delay(10); return true;
}

bool IrModule::loopFileCodes(Context& ctx) {
    if (blasting_) {
        if (millis() - lastBlastMs_ > 120) {
            if (blastIdx_ < codes_.size()) { sendCode(ctx, codes_[blastIdx_++]); lastBlastMs_ = millis(); }
            else { blasting_ = false; blastIdx_ = 0; }
        }
        ctx.display->clear(); ctx.display->header("IR blast");
        ctx.display->printf(0, 0xFFE0, "%u/%u", (unsigned)blastIdx_, (unsigned)codes_.size());
        ctx.display->footer("Esc=stop");
        if (ctx.input->poll().key == Key::Back) { blasting_ = false; blastIdx_ = 0; }
        return true;
    }
    if (codes_.empty()) {
        ctx.display->clear(); ctx.display->header("IR codes");
        ctx.display->line(0, "No codes in file.", 0xF800);
        ctx.display->footer("Esc=back");
        if (ctx.input->poll().key == Key::Back) view_ = View::Library;
        delay(20); return true;
    }
    ctx.display->clear(); ctx.display->header("IR codes");
    int rows = ctx.display->bodyRows();
    int first = sel_ >= rows ? sel_ - rows + 1 : 0;
    for (int r = 0; r < rows && (first + r) < (int)codes_.size(); ++r) {
        int i = first + r; bool s = (i == sel_);
        ctx.display->line(r, ((s ? "> " : "  ") + codes_[i].name).c_str(), s ? 0xFFE0 : 0xFFFF);
    }
    ctx.display->footer("Enter=send  B=blast  Esc=back");
    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   sel_ = (sel_ + codes_.size() - 1) % codes_.size(); break;
        case Key::Down: sel_ = (sel_ + 1) % codes_.size(); break;
        case Key::Enter: sendCode(ctx, codes_[sel_]); break;
        case Key::Back:  view_ = View::Library; break;
        case Key::Char:  if (e.ch=='b'||e.ch=='B') { blasting_ = true; blastIdx_ = 0; } break;
        default: break;
    }
    delay(10); return true;
}

bool IrModule::loopProto(Context& ctx) {
    static const char* protos[] = {"NEC", "NEC-ext", "Samsung", "Sony-12",
                                   "Sony-20", "RC5", "NEC brute (sweep)"};
    const int N = 7;
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

            if (protoSel_ == 6) {                    // NEC brute sweep
                for (int cmd = 0; cmd <= 255; ++cmd) {
                    irtx::sendRaw(irtx::encodeNEC(addr & 0xFF, cmd), 38000);
                    if ((cmd & 7) == 0) {
                        ctx.display->clear(); ctx.display->header("IR NEC brute");
                        ctx.display->printf(0, 0xFFFF, "addr 0x%02lX", addr & 0xFF);
                        ctx.display->printf(1, 0xFFE0, "cmd 0x%02X (%d/256)", cmd, cmd + 1);
                        ctx.display->footer("Esc=stop"); ctx.display->present();
                    }
                    if (ctx.input->poll().key == Key::Back) break;
                    delay(60);
                }
                break;
            }

            String c;
            if (!ctx.input->readLine("cmd (hex):", c)) break;
            long cmd = parseHex(c);
            static const IrProto map[] = {P_NEC, P_NECX, P_SAMSUNG, P_SONY12, P_SONY20, P_RC5};
            uint16_t freq; auto t = encodeFor(map[protoSel_], addr, cmd & 0xFF, freq);
            irtx::sendRaw(t, freq);
            last_.name = String(protos[protoSel_]); last_.freq = freq; last_.timings = t;
            if (ctx.log) ctx.log->tx("ir", "%s addr=0x%lX cmd=0x%lX", protos[protoSel_], addr, cmd);
            ctx.display->clear(); ctx.display->header("IR: protocol");
            ctx.display->line(2, "Sent. (Save in menu)", 0x07E0);
            ctx.display->present();
            delay(500);
            break;
        }
        default: break;
    }
    delay(10);
    return true;
}

REGISTER_MODULE(IrModule);
