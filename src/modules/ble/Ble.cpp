#include "Ble.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/Logger.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

void BleModule::enter(Context& ctx) {
    BLEDevice::init("");
    devs_.clear(); sel_ = 0;
    doScan(ctx);
}

void BleModule::doScan(Context& ctx) {
    ctx.display->clear();
    ctx.display->header("BLE Scan");
    ctx.display->line(0, "Scanning 5s...", 0xFFE0);

    BLEScan* scan = BLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);
    // arduino-esp32 2.x returns BLEScanResults by value (3.x returns a pointer).
    BLEScanResults res = scan->start(5, false);

    devs_.clear();
    int n = res.getCount();
    for (int i = 0; i < n; ++i) {
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

bool BleModule::loop(Context& ctx) {
    if (devs_.empty()) {
        ctx.display->line(0, "No devices. R=rescan", 0x8410);
        ctx.display->footer("Esc=back");
        KeyEvent e = ctx.input->poll();
        if (e.key == Key::Back) return false;
        if (e.key == Key::Char && (e.ch=='r'||e.ch=='R')) doScan(ctx);
        delay(20); return true;
    }

    const Dev& d = devs_[sel_];
    ctx.display->clear();
    ctx.display->header("BLE Scan");
    ctx.display->printf(0, 0xFFFF, "%s", d.name.c_str());
    ctx.display->printf(1, 0x8410, "%s", d.addr.c_str());
    ctx.display->printf(2, 0x07E0, "%d dBm   %d/%d", d.rssi, sel_+1, (int)devs_.size());
    ctx.display->line(5, "R=rescan  (spam/HID=TODO)", 0x8410);
    ctx.display->footer("Up/Down  Esc=back");

    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   sel_ = (sel_ + devs_.size() - 1) % devs_.size(); break;
        case Key::Down: sel_ = (sel_ + 1) % devs_.size(); break;
        case Key::Back: return false;
        case Key::Char: if (e.ch=='r'||e.ch=='R') doScan(ctx); break;
        default: break;
    }
    delay(10);
    return true;
}

void BleModule::exit(Context& ctx) {
    BLEDevice::deinit(true);
}

// --- Active hooks: NOT implemented. Authorized lab use only. ---------------
void BleModule::advSpam(Context& ctx)   { (void)ctx; /* TODO: pairing-popup adverts, own devices */ }
void BleModule::hidInject(Context& ctx) { (void)ctx; /* TODO: BLE HID keyboard, own devices */ }

REGISTER_MODULE(BleModule);
