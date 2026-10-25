// Ble.h — Bluetooth LE on the ESP32-S3.
//
//   Scan   passive BLE device scan (name/address/RSSI)
//   HID    "BadBLE" — advertise as a BLE keyboard and run a DuckyScript from SD
//          against a device you pair with. Compiled only when
//          HUGINN_ENABLE_BLE_HID is defined; not built in the default profile
//          yet (the BLE-stack coexistence with the scanner needs verifying).
//          Runs on this hardware as-is — no add-on needed. Authorized use only.
//
// Advertisement "spam" (pairing-popup flooding of every device in range) is a
// mass-targeting nuisance and is intentionally not shipped.
#pragma once
#include "../../core/Module.h"
#include <vector>

class BleModule : public Module {
public:
    const char* name() const override { return "BLE"; }
    Category    category() const override { return Category::Bluetooth; }
    const char* description() const override { return "Scan devices / HID inject"; }
    bool        requiresAuthorization() const override { return true; }

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;
    void exit(Context& ctx) override;

private:
    struct Dev {
        String name; String addr; int rssi = 0;
        String svc;            // advertised service UUID (if any)
        String mfg;            // manufacturer data (hex) / vendor hint
        int    appearance = 0;
    };
    enum class View { Menu, Scan, Detail, Hid };
    View view_ = View::Menu;
    int  menuSel_ = 0;

    std::vector<Dev> devs_;
    int  scanSel_ = 0;
    uint32_t lastScanMs_ = 0;
    void doScan(Context& ctx);
    bool loopScan(Context& ctx);
    bool loopDetail(Context& ctx);

#ifdef HUGINN_ENABLE_BLE_HID
    bool hidStarted_ = false;
    bool loopHid(Context& ctx);
    void runBleScript(Context& ctx, const String& path);
#endif
};
