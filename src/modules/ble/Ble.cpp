#include "Ble.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/SdCard.h"
#include "../../core/Config.h"
#include "../../core/Logger.h"
#include "../../hal/Vendor.h"
#include <WiFi.h>
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
    menuSel_ = 0;
#ifdef HUGINN_ENABLE_BLE_HID
    view_ = View::Menu;
#else
    // Free the 2.4 GHz radio for BLE — WiFi left on badly degrades scanning.
    WiFi.mode(WIFI_OFF);
    BLEDevice::init("");
    devs_.clear(); scanSel_ = 0; lastScanMs_ = 0;
    view_ = View::Scan;
#endif
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
        case View::Scan:   return loopScan(ctx);
#ifndef HUGINN_ENABLE_BLE_HID
        case View::Detail: return loopDetail(ctx);
#else
        case View::Detail: view_ = View::Menu; break;
        case View::Hid:    return loopHid(ctx);
#endif
        case View::Menu: break;
        default: break;
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
// One 2s sweep; merges results into the live list (dedup by address) and grabs
// whatever the advertisement exposes (name, service UUID, manufacturer, type).
void BleModule::doScan(Context& ctx) {
    ctx.display->footer("scanning BLE...");
    ctx.display->present();
    BLEScan* scan = BLEDevice::getScan();
    scan->setActiveScan(true); scan->setInterval(160); scan->setWindow(160);
    BLEScanResults res = scan->start(3, false);
    for (int i = 0; i < res.getCount(); ++i) {
        BLEAdvertisedDevice d = res.getDevice(i);
        String addr = String(d.getAddress().toString().c_str());
        int rssi = d.haveRSSI() ? d.getRSSI() : -100;
        int slot = -1;
        for (int j = 0; j < (int)devs_.size(); ++j) if (devs_[j].addr == addr) { slot = j; break; }
        if (slot < 0) { devs_.push_back(Dev()); slot = devs_.size() - 1; devs_[slot].addr = addr; }
        Dev& dev = devs_[slot];
        dev.rssi = rssi;
        if (d.haveName()) dev.name = String(d.getName().c_str());
        if (d.haveServiceUUID()) dev.svc = String(d.getServiceUUID().toString().c_str());
        if (d.haveAppearance()) dev.appearance = d.getAppearance();
        if (d.haveManufacturerData()) {
            std::string raw = d.getManufacturerData();
            String hex; for (size_t k = 0; k < raw.size() && k < 8; ++k) {
                char b[4]; snprintf(b, sizeof(b), "%02X", (uint8_t)raw[k]); hex += b; }
            dev.mfg = hex;
            if (raw.size() >= 2) {   // first 2 bytes = company ID (little-endian)
                uint16_t cid = (uint8_t)raw[0] | ((uint8_t)raw[1] << 8);
                dev.vendor = vendor::forCompanyId(cid);
            }
        }
    }
    scan->clearResults();
}

bool BleModule::loopScan(Context& ctx) {
    if (millis() - lastScanMs_ > 300) { lastScanMs_ = millis(); doScan(ctx); }
    if (scanSel_ >= (int)devs_.size()) scanSel_ = devs_.empty() ? 0 : devs_.size() - 1;

    ctx.display->clear();
    char title[24]; snprintf(title, sizeof(title), "BLE  %d dev", (int)devs_.size());
    ctx.display->header(title);
    if (devs_.empty()) {
        ctx.display->line(0, "scanning... none yet", 0x8410);
    } else {
        int rows = ctx.display->bodyRows();
        int first = scanSel_ >= rows ? scanSel_ - rows + 1 : 0;
        for (int r = 0; r < rows && (first + r) < (int)devs_.size(); ++r) {
            int i = first + r; const Dev& d = devs_[i]; bool s = (i == scanSel_);
            String nm = d.name.length() ? d.name
                      : (d.vendor.length() ? d.vendor : d.addr.substring(9));
            if (nm.length() > 14) nm = nm.substring(0, 14);
            char buf[44];
            snprintf(buf, sizeof(buf), "%s%-14s %d", s ? ">" : " ", nm.c_str(), d.rssi);
            ctx.display->line(r, buf, s ? 0xFFE0 : (d.rssi > -60 ? 0x07E0 : 0xFFFF));
        }
    }
    ctx.display->footer("Up/Dn  Enter=detail  Esc=back");

    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   if (scanSel_ > 0) scanSel_--; break;
        case Key::Down: if (scanSel_ < (int)devs_.size() - 1) scanSel_++; break;
        case Key::Enter: if (!devs_.empty()) view_ = View::Detail; break;
        case Key::Back:  view_ = View::Menu; return true;
        default: break;
    }
    delay(10); return true;
}

bool BleModule::loopDetail(Context& ctx) {
    if (devs_.empty() || scanSel_ >= (int)devs_.size()) { view_ = View::Scan; return true; }
    const Dev& d = devs_[scanSel_];
    ctx.display->clear();
    ctx.display->header("BLE device");
    ctx.display->printf(0, 0xFFFF, "%s", d.name.length() ? d.name.c_str() : "<no name>");
    ctx.display->printf(1, 0x07E0, "vendor: %s", d.vendor.length() ? d.vendor.c_str() : "?");
    ctx.display->printf(2, 0x8410, "%s", d.addr.c_str());
    ctx.display->printf(3, 0xFFFF, "RSSI %d dBm", d.rssi);
    ctx.display->printf(4, 0xFFFF, "svc: %s", d.svc.length() ? d.svc.c_str() : "-");
    ctx.display->printf(5, 0x8410, "mfg: %s  app: %d",
                        d.mfg.length() ? d.mfg.c_str() : "-", d.appearance);
    ctx.display->footer("Esc=back to list");
    if (ctx.input->poll().key == Key::Back) view_ = View::Scan;
    delay(20); return true;
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
