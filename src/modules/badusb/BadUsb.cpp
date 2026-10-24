#include "BadUsb.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/SdCard.h"
#include "../../core/Config.h"
#include "../../core/Logger.h"
#include <FS.h>
#include <USB.h>
#include <USBHIDKeyboard.h>

static USBHIDKeyboard s_kb;

// Map a DuckyScript key token to a HID keycode usable with s_kb.press().
static int tokenToKey(const String& t) {
    if (t == "ENTER")  return KEY_RETURN;
    if (t == "TAB")    return KEY_TAB;
    if (t == "ESC" || t == "ESCAPE") return KEY_ESC;
    if (t == "SPACE")  return ' ';
    if (t == "GUI" || t == "WINDOWS") return KEY_LEFT_GUI;
    if (t == "CTRL" || t == "CONTROL") return KEY_LEFT_CTRL;
    if (t == "ALT")    return KEY_LEFT_ALT;
    if (t == "SHIFT")  return KEY_LEFT_SHIFT;
    if (t == "UP" || t == "UPARROW")       return KEY_UP_ARROW;
    if (t == "DOWN" || t == "DOWNARROW")   return KEY_DOWN_ARROW;
    if (t == "LEFT" || t == "LEFTARROW")   return KEY_LEFT_ARROW;
    if (t == "RIGHT" || t == "RIGHTARROW") return KEY_RIGHT_ARROW;
    if (t == "DELETE" || t == "DEL") return KEY_DELETE;
    if (t == "BACKSPACE") return KEY_BACKSPACE;
    if (t.length() == 1) return (int)t[0];
    // F-keys
    if (t.length() >= 2 && t[0] == 'F') {
        int n = t.substring(1).toInt();
        if (n >= 1 && n <= 12) return KEY_F1 + (n - 1);
    }
    return 0;
}

void BadUsbModule::enter(Context& ctx) {
    ctx.display->clear();
    ctx.display->header("BadUSB");
    listPayloads(ctx);

    // Bring up the HID keyboard. On the S3 this enumerates as a USB keyboard.
    s_kb.begin();
    USB.begin();
    hidReady_ = true;
}

void BadUsbModule::listPayloads(Context& ctx) {
    files_.clear(); sel_ = 0;
    String dir = ctx.config->getString("badusb.payload_dir", "/huginn/payloads/badusb");
    if (!ctx.sd || !ctx.sd->exists(dir.c_str())) return;
    File d = ctx.sd->fs().open(dir.c_str());
    if (!d) return;
    for (File e = d.openNextFile(); e; e = d.openNextFile()) {
        if (!e.isDirectory()) files_.push_back(String(dir) + "/" + e.name());
        e.close();
    }
    d.close();
}

void BadUsbModule::pressCombo(const String& tokens) {
    // Space-separated modifiers + final key, e.g. "GUI r" or "CTRL ALT DELETE".
    std::vector<int> keys;
    int start = 0;
    while (start < (int)tokens.length()) {
        int sp = tokens.indexOf(' ', start);
        if (sp < 0) sp = tokens.length();
        String tok = tokens.substring(start, sp); tok.trim();
        if (tok.length()) { int k = tokenToKey(tok); if (k) keys.push_back(k); }
        start = sp + 1;
    }
    for (int k : keys) s_kb.press(k);
    delay(40);
    s_kb.releaseAll();
}

void BadUsbModule::execLine(const String& raw, uint32_t& defaultDelay) {
    String line = raw; line.trim();
    if (line.length() == 0 || line.startsWith("REM")) return;

    if (line.startsWith("DEFAULT_DELAY ") || line.startsWith("DEFAULTDELAY ")) {
        defaultDelay = line.substring(line.indexOf(' ') + 1).toInt();
        return;
    }
    if (line.startsWith("DELAY ")) { delay(line.substring(6).toInt()); return; }
    if (line.startsWith("STRING ")) { s_kb.print(line.substring(7)); }
    else if (line.startsWith("STRINGLN ")) { s_kb.println(line.substring(9)); }
    else { pressCombo(line); }     // ENTER, GUI r, CTRL ALT DELETE, etc.

    if (defaultDelay) delay(defaultDelay);
}

void BadUsbModule::runPayload(Context& ctx, const String& path) {
    File f = ctx.sd->fs().open(path.c_str(), FILE_READ);
    if (!f) return;
    if (ctx.log) ctx.log->tx("badusb", "running payload %s", path.c_str());
    ctx.display->clear();
    ctx.display->header("BadUSB");
    ctx.display->line(0, "Running payload...", 0xFFE0);

    uint32_t defaultDelay = 0;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        execLine(line, defaultDelay);
        // Allow abort between lines.
        if (ctx.input->poll().key == Key::Back) { ctx.display->line(1, "Aborted.", 0xF800); break; }
    }
    f.close();
    s_kb.releaseAll();
    ctx.display->line(2, "Done. Esc=back", 0x07E0);
}

bool BadUsbModule::loop(Context& ctx) {
    if (files_.empty()) {
        ctx.display->line(0, "No payloads found.", 0xF800);
        ctx.display->line(1, "Add .txt to", 0x8410);
        ctx.display->line(2, "/huginn/payloads/badusb", 0x8410);
        ctx.display->footer("Esc=back");
        if (ctx.input->poll().key == Key::Back) return false;
        delay(20); return true;
    }

    ctx.display->clear();
    ctx.display->header("BadUSB");
    ctx.display->printf(0, 0xFFFF, "%s", files_[sel_].c_str());
    ctx.display->printf(1, 0x8410, "%d/%d  HID:%s",
                        sel_+1, (int)files_.size(), hidReady_ ? "up" : "down");
    ctx.display->line(3, "Enter=RUN (authorized!)", 0xFFE0);
    ctx.display->footer("Up/Down  Esc=back");

    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   sel_ = (sel_ + files_.size() - 1) % files_.size(); break;
        case Key::Down: sel_ = (sel_ + 1) % files_.size(); break;
        case Key::Enter:
            runPayload(ctx, files_[sel_]);
            // wait for Esc after running
            while (ctx.input->poll().key != Key::Back) delay(20);
            break;
        case Key::Back: return false;
        default: break;
    }
    delay(10);
    return true;
}

void BadUsbModule::exit(Context& ctx) { s_kb.releaseAll(); }

REGISTER_MODULE(BadUsbModule);
