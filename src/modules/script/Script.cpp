#include "Script.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/SdCard.h"
#include "../../core/Config.h"
#include "../../core/Logger.h"
#include "../../hal/IrTx.h"
#include <FS.h>

void ScriptModule::enter(Context& ctx) {
    view_ = View::List; sel_ = 0; outN_ = 0;
    listScripts(ctx);
}

void ScriptModule::listScripts(Context& ctx) {
    files_.clear();
    String dir = ctx.config->getString("script.dir", "/huginn/scripts");
    if (!ctx.sd || !ctx.sd->exists(dir.c_str())) return;
    File d = ctx.sd->fs().open(dir.c_str());
    for (File e = d.openNextFile(); e; e = d.openNextFile()) {
        if (!e.isDirectory()) {
            String n = e.name();
            int sl = n.lastIndexOf('/'); if (sl >= 0) n = n.substring(sl + 1);
            if (n.endsWith(".hsc") || n.endsWith(".txt")) files_.push_back(dir + "/" + n);
        }
        e.close();
    }
    if (d) d.close();
}

void ScriptModule::pushOut(const String& s) {
    if (outN_ < 6) out_[outN_++] = s;
    else { for (int i = 0; i < 5; ++i) out_[i] = out_[i+1]; out_[5] = s; }
}

// Blast every raw code found in a Flipper-style .ir file.
void ScriptModule::execIr(Context& ctx, const String& path) {
    if (!ctx.sd || !ctx.sd->exists(path.c_str())) { pushOut("IR: not found"); return; }
    File f = ctx.sd->fs().open(path.c_str(), FILE_READ);
    if (!f) return;
    uint16_t freq = 38000; std::vector<uint16_t> t;
    int sent = 0;
    while (f.available()) {
        String line = f.readStringUntil('\n'); line.trim();
        if (line.startsWith("frequency:")) freq = line.substring(10).toInt();
        else if (line.startsWith("data:")) {
            t.clear();
            String d = line.substring(5); d.trim();
            int start = 0;
            while (start < (int)d.length()) {
                int sp = d.indexOf(' ', start); if (sp < 0) sp = d.length();
                String tok = d.substring(start, sp);
                if (tok.length()) t.push_back((uint16_t)tok.toInt());
                start = sp + 1;
            }
            if (!t.empty()) { irtx::sendRaw(t, freq); sent++; delay(80); }
        }
    }
    f.close();
    pushOut(String("IR sent ") + sent);
    if (ctx.log) ctx.log->tx("script", "IR %s (%d codes)", path.c_str(), sent);
}

void ScriptModule::run(Context& ctx, const String& path) {
    File f = ctx.sd->fs().open(path.c_str(), FILE_READ);
    if (!f) return;
    std::vector<String> lines;
    while (f.available()) { String l = f.readStringUntil('\n'); l.trim(); lines.push_back(l); }
    f.close();

    outN_ = 0;
    if (ctx.log) ctx.log->info("Script run: %s (%u lines)", path.c_str(), (unsigned)lines.size());

    struct Frame { int start; int remaining; };
    std::vector<Frame> loops;
    int pc = 0;
    bool abort = false;

    auto draw = [&](const char* status) {
        ctx.display->clear(); ctx.display->header("Script");
        ctx.display->printf(0, 0xFFE0, "%s  line %d/%d", status, pc + 1, (int)lines.size());
        for (int i = 0; i < outN_; ++i) ctx.display->line(2 + i, out_[i].c_str(), 0xFFFF);
        ctx.display->footer("Esc=stop");
    };

    while (pc < (int)lines.size() && !abort) {
        String line = lines[pc];
        if (line.length() == 0 || line.startsWith("#")) { pc++; continue; }

        int sp = line.indexOf(' ');
        String cmd = sp < 0 ? line : line.substring(0, sp);
        String arg = sp < 0 ? ""   : line.substring(sp + 1);
        cmd.toUpperCase();

        if (cmd == "REPEAT") {
            int n = arg.toInt(); if (n < 1) n = 1;
            loops.push_back({pc + 1, n});
        } else if (cmd == "END") {
            if (!loops.empty()) {
                if (--loops.back().remaining > 0) { pc = loops.back().start; draw("loop"); continue; }
                loops.pop_back();
            }
        } else if (cmd == "DELAY") {
            uint32_t ms = (uint32_t)arg.toInt();
            uint32_t t0 = millis();
            while (millis() - t0 < ms) {           // stay responsive to Esc
                if (ctx.input->poll().key == Key::Back) { abort = true; break; }
                delay(10);
            }
        } else if (cmd == "PRINT") {
            pushOut(arg); draw("run");
        } else if (cmd == "LOG") {
            if (ctx.log) ctx.log->info("script: %s", arg.c_str());
        } else if (cmd == "IR") {
            execIr(ctx, arg); draw("run");
        } else {
            pushOut(String("? ") + cmd);
        }

        if (ctx.input->poll().key == Key::Back) abort = true;
        pc++;
        draw(abort ? "stop" : "run");
    }
    pushOut(abort ? "-- aborted --" : "-- done --");
}

bool ScriptModule::loop(Context& ctx) {
    if (view_ == View::Run) {
        ctx.display->clear(); ctx.display->header("Script");
        for (int i = 0; i < outN_; ++i) ctx.display->line(i, out_[i].c_str(), 0xFFFF);
        ctx.display->footer("Esc=back");
        if (ctx.input->poll().key == Key::Back) { view_ = View::List; }
        delay(20); return true;
    }
    if (view_ == View::Edit) return loopEdit(ctx);

    // Row 0 is always "[+ new script]"; real files follow.
    int total = (int)files_.size() + 1;
    if (sel_ >= total) sel_ = total - 1;
    ctx.display->clear(); ctx.display->header("Scripts");
    int rows = ctx.display->bodyRows();
    int first = sel_ >= rows ? sel_ - rows + 1 : 0;
    for (int r = 0; r < rows && (first + r) < total; ++r) {
        int i = first + r; bool s = (i == sel_);
        String n;
        if (i == 0) n = "[+ new script]";
        else { n = files_[i-1]; int sl = n.lastIndexOf('/'); if (sl >= 0) n = n.substring(sl + 1); }
        ctx.display->line(r, ((s ? "> " : "  ") + n).c_str(),
                          s ? 0xFFE0 : (i == 0 ? 0x07E0 : 0xFFFF));
    }
    ctx.display->footer("Enter=run  E=edit  Esc=back");

    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   sel_ = (sel_ + total - 1) % total; break;
        case Key::Down: sel_ = (sel_ + 1) % total; break;
        case Key::Enter:
            if (sel_ == 0) openEditor(ctx, "");                 // new file
            else { view_ = View::Run; run(ctx, files_[sel_-1]); }
            break;
        case Key::Char:
            if ((e.ch == 'e' || e.ch == 'E') && sel_ > 0) openEditor(ctx, files_[sel_-1]);
            break;
        case Key::Back: return false;
        default: break;
    }
    delay(10);
    return true;
}

// --- On-device editor ------------------------------------------------------
void ScriptModule::openEditor(Context& ctx, const String& path) {
    edLines_.clear(); edCur_ = 0; edDirty_ = false;
    if (path.length()) {
        edPath_ = path;
        File f = ctx.sd->fs().open(path.c_str(), FILE_READ);
        while (f && f.available()) { String l = f.readStringUntil('\n'); l.trim(); edLines_.push_back(l); }
        if (f) f.close();
    } else {
        // New file: ask for a name under the scripts dir.
        String dir = ctx.config->getString("script.dir", "/huginn/scripts");
        String name;
        if (!ctx.input->readLine("new name:", name) || name.length() == 0) return;
        if (!name.endsWith(".hsc")) name += ".hsc";
        edPath_ = dir + "/" + name;
        if (ctx.sd) ctx.sd->mkdirs(dir.c_str());
    }
    if (edLines_.empty()) edLines_.push_back("");
    view_ = View::Edit;
}

void ScriptModule::saveEditor(Context& ctx) {
    if (!ctx.sd) return;
    File f = ctx.sd->fs().open(edPath_.c_str(), FILE_WRITE);
    if (!f) return;
    for (auto& l : edLines_) f.println(l);
    f.close();
    edDirty_ = false;
    if (ctx.log) ctx.log->info("Script saved: %s (%u lines)", edPath_.c_str(), (unsigned)edLines_.size());
    listScripts(ctx);                 // refresh the file list
}

bool ScriptModule::loopEdit(Context& ctx) {
    if (edCur_ >= (int)edLines_.size()) edCur_ = edLines_.empty() ? 0 : edLines_.size() - 1;

    ctx.display->clear();
    char title[28];
    String base = edPath_; int sl = base.lastIndexOf('/'); if (sl >= 0) base = base.substring(sl + 1);
    snprintf(title, sizeof(title), "%s%s", base.c_str(), edDirty_ ? " *" : "");
    ctx.display->header(title);

    int rows = ctx.display->bodyRows() - 1;
    int first = edCur_ >= rows ? edCur_ - rows + 1 : 0;
    for (int r = 0; r < rows && (first + r) < (int)edLines_.size(); ++r) {
        int i = first + r; bool s = (i == edCur_);
        String l = edLines_[i]; if (l.length() > 26) l = l.substring(0, 26);
        char buf[40];
        snprintf(buf, sizeof(buf), "%s%s", s ? ">" : " ", l.c_str());
        ctx.display->line(r, buf, s ? 0xFFE0 : 0xFFFF);
    }
    ctx.display->line(rows, "Enter=edit N=new D=del S=save R=run", 0x5D9F);
    ctx.display->footer("Up/Dn move   Esc=back");

    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   if (edCur_ > 0) edCur_--; break;
        case Key::Down: if (edCur_ < (int)edLines_.size() - 1) edCur_++; break;
        case Key::Enter: {                               // edit current line
            String v = edLines_[edCur_];
            if (ctx.input->readLine("line:", v, false, true)) { edLines_[edCur_] = v; edDirty_ = true; }
            break;
        }
        case Key::Char:
            if (e.ch == 'n' || e.ch == 'N') {
                edLines_.insert(edLines_.begin() + edCur_ + 1, String(""));
                edCur_++; edDirty_ = true;
            } else if (e.ch == 'd' || e.ch == 'D') {
                if (edLines_.size() > 1) { edLines_.erase(edLines_.begin() + edCur_); edDirty_ = true; }
                else edLines_[0] = "";
            } else if (e.ch == 's' || e.ch == 'S') {
                saveEditor(ctx);
            } else if (e.ch == 'r' || e.ch == 'R') {
                saveEditor(ctx); view_ = View::Run; run(ctx, edPath_);
            }
            break;
        case Key::Back: view_ = View::List; break;
        default: break;
    }
    delay(10);
    return true;
}

REGISTER_MODULE(ScriptModule);
