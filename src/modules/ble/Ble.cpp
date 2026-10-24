#include "Ble.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/SdCard.h"
#include "../../core/Config.h"
#include "../../core/Logger.h"
#include <FS.h>

#ifdef HUGINN_ENABLE_BLE_HID
#include <BleKeyboard.h>
static BleKeyboard s_bleKb("Huginn", "Huginn", 100);
#else
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#endif

// NOTE: the BLE scanner (bluedroid BLEDevice) and the BleKeyboard HID server
// can't run at once, so the HID build drops the scanner to avoid a stack clash.

void BleModule::enter(Context& ctx) {
    view_ = View::Menu; menuSel_ = 0;
}

void BleModule::exit(Context& ctx) {
#ifdef HUGINN_ENABLE_BLE_HID
    if (hidStarted_) { s_bleKb.end(); hidStarted_ = false; }
#else
    BLEDevice::deinit(true);
#endif
}

bool BleModule::loop(Context& ctx) {
    switch (view_) {
        case View::Scan: return loopScan(ctx);
#ifdef HUGINN_ENABLE_BLE_HID
        case View::Hid:  return loopHid(ctx);
#else
        case View::Hid:  view_ = View::Menu; break;
#endif
        case View::Menu: break;
    }

#ifdef HUGINN_ENABLE_BLE_HID
    static const char* items[] = {"HID inject (BadBLE)"};
    const int N = 1;   // scanner omitted in HID build (stack conflict)
#else
    static const char* items[] = {"Scan devices"};
    const int N = 1;
#endif
    ctx.display->list(items, N, menuSel_, "BLE", "Enter=open  Esc=back");
    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   menuSel_ = (menuSel_ + N - 1) % N; break;
        case Key::Down: menuSel_ = (menuSel_ + 1) % N; break;
        case Key::Back: return false;
        case Key::Enter:
#ifdef HUGINN_ENABLE_BLE_HID
            view_ = View::Hid;
#else
            BLEDevice::init(""); doScan(ctx); view_ = View::Scan;
#endif
            break;
        default: break;
    }
    delay(10);
    return true;
}

// --- Scan (core build) -----------------------------------------------------
#ifndef HUGINN_ENABLE_BLE_HID
void BleModule::doScan(Context& ctx) {
    ctx.display->clear(); ctx.display->header("BLE Scan");
    ctx.display->line(0, "Scanning 5s...", 0xFFE0);
    BLEScan* scan = BLEDevice::getScan();
    scan->setActiveScan(true); scan->setInterval(100); scan->setWindow(99);
    BLEScanResults res = scan->start(5, false);
    devs_.clear(); scanSel_ = 0;
    for (int i = 0; i < res.getCount(); ++i) {
        BLEAdvertisedDevice d = res.getDevice(i);
        Dev dev;
        dev.name = d.haveName() ? String(d.getName().c_str()) : String("<unknown>");
        dev.addr = String(d.getAddress().toString().c_str());
        dev.rssi = d.haveRSSI() ? d.getRSSI() : 0;
        devs_.push_back(dev);
    }
    scan->clearResults();
    if (ctx.log) ctx.log->info("BLE scan: %d devices", (int)devs_.size());
}

bool BleModule::loopScan(Context& ctx) {
    if (devs_.empty()) {
        ctx.display->clear(); ctx.display->header("BLE Scan");
        ctx.display->line(0, "No devices. R=rescan", 0x8410);
        ctx.display->footer("Esc=back");
        KeyEvent e = ctx.input->poll();
        if (e.key == Key::Back) view_ = View::Menu;
        if (e.key == Key::Char && (e.ch=='r'||e.ch=='R')) doScan(ctx);
        delay(20); return true;
    }
    const Dev& d = devs_[scanSel_];
    ctx.display->clear(); ctx.display->header("BLE Scan");
    ctx.display->printf(0, 0xFFFF, "%s", d.name.c_str());
    ctx.display->printf(1, 0x8410, "%s", d.addr.c_str());
    ctx.display->printf(2, 0x07E0, "%d dBm   %d/%d", d.rssi, scanSel_+1, (int)devs_.size());
    ctx.display->footer("Up/Dn  R=rescan  Esc=back");
    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   scanSel_ = (scanSel_ + devs_.size() - 1) % devs_.size(); break;
        case Key::Down: scanSel_ = (scanSel_ + 1) % devs_.size(); break;
        case Key::Back: view_ = View::Menu; break;
        case Key::Char: if (e.ch=='r'||e.ch=='R') doScan(ctx); break;
        default: break;
    }
    delay(10); return true;
}
#else
// Scan is unavailable in the HID build; keep a stub so the view compiles.
void BleModule::doScan(Context&) {}
bool BleModule::loopScan(Context& ctx) { view_ = View::Menu; return true; }
#endif

// --- HID inject (full build) -----------------------------------------------
#ifdef HUGINN_ENABLE_BLE_HID
void BleModule::runBleScript(Context& ctx, const String& path) {
    File f = ctx.sd->fs().open(path.c_str(), FILE_READ);
    if (!f) return;
    if (ctx.log) ctx.log->tx("ble-hid", "running %s", path.c_str());
    uint32_t defDelay = 0;
    while (f.available()) {
        String line = f.readStringUntil('\n'); line.trim();
        if (line.length() == 0 || line.startsWith("REM")) continue;
        if (line.startsWith("DEFAULT_DELAY ")) { defDelay = line.substring(14).toInt(); continue; }
        if (line.startsWith("DELAY ")) { delay(line.substring(6).toInt()); continue; }
        if (line.startsWith("STRING "))  { s_bleKb.print(line.substring(7)); }
        else if (line == "ENTER")        { s_bleKb.write(KEY_RETURN); }
        else if (line.startsWith("GUI ")) {
            s_bleKb.press(KEY_LEFT_GUI); s_bleKb.press(line.substring(4)[0]);
            delay(30); s_bleKb.releaseAll();
        }
        if (defDelay) delay(defDelay);
        if (ctx.input->poll().key == Key::Back) break;
    }
    f.close();
    s_bleKb.releaseAll();
}

bool BleModule::loopHid(Context& ctx) {
    if (!hidStarted_) { s_bleKb.begin(); hidStarted_ = true; }

    ctx.display->clear(); ctx.display->header("BadBLE HID");
    if (!s_bleKb.isConnected()) {
        ctx.display->line(0, "Advertising as keyboard", 0xFFE0);
        ctx.display->line(1, "Pair 'Huginn' on target", 0xFFFF);
        ctx.display->line(2, "(authorized device only)", 0x8410);
        ctx.display->footer("Esc=back");
        if (ctx.input->poll().key == Key::Back) view_ = View::Menu;
        delay(50); return true;
    }
    ctx.display->line(0, "Connected.", 0x07E0);
    ctx.display->line(1, "Enter=run payload", 0xFFE0);
    ctx.display->footer("Esc=back");
    KeyEvent e = ctx.input->poll();
    if (e.key == Key::Back) view_ = View::Menu;
    if (e.key == Key::Enter) {
        String dir = ctx.config->getString("badusb.payload_dir", "/huginn/payloads/badusb");
        // Run the first payload found; a picker can come later.
        File d = ctx.sd->fs().open(dir.c_str());
        for (File fe = d.openNextFile(); fe; fe = d.openNextFile()) {
            if (!fe.isDirectory()) {
                String n = fe.name();
                int sl = n.lastIndexOf('/'); if (sl >= 0) n = n.substring(sl + 1);
                String p = dir + "/" + n; fe.close();
                runBleScript(ctx, p); break;
            }
            fe.close();
        }
        if (d) d.close();
    }
    delay(10); return true;
}
#endif

REGISTER_MODULE(BleModule);
