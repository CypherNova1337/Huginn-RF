// Ble.h — Bluetooth LE on the ESP32-S3.
//
// Implemented: passive BLE device scan (name/address/RSSI). Declared-but-stub:
// advertisement "spam" and HID keyboard emulation ("BadBLE"). Those are left as
// hooks because they actively interfere with / inject into other devices —
// implement only for your own devices in an isolated setting, and log each.
#pragma once
#include "../../core/Module.h"
#include <vector>

class BleModule : public Module {
public:
    const char* name() const override { return "BLE Scan"; }
    Category    category() const override { return Category::Bluetooth; }
    const char* description() const override { return "Scan BLE; spam/HID hooks (lab)"; }
    bool        requiresAuthorization() const override { return true; }

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;
    void exit(Context& ctx) override;

private:
    struct Dev { String name; String addr; int rssi; };
    void doScan(Context& ctx);

    // --- Active hooks (NOT implemented; authorized lab use only) ----------
    void advSpam(Context& ctx);     // TODO: vendor pairing-popup advertisements
    void hidInject(Context& ctx);   // TODO: emulate a BLE HID keyboard

    std::vector<Dev> devs_;
    int sel_ = 0;
};
