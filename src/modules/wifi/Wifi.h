// Wifi.h — 2.4 GHz WiFi auditing on the ESP32-S3 radio.
//
// Implemented here: passive AP scan (SSID/BSSID/channel/RSSI/encryption) and a
// per-channel activity view. The active-attack actions common to pentest
// firmwares (EAPOL/PMKID capture, deauth, evil-portal) are declared as hooks
// and left as TODOs for you to implement against your own lab — each is gated
// behind requiresAuthorization() and should log every frame it transmits.
#pragma once
#include "../../core/Module.h"
#include <vector>

class WifiModule : public Module {
public:
    const char* name() const override { return "WiFi Scan"; }
    Category    category() const override { return Category::WiFi; }
    const char* description() const override { return "Scan APs; audit hooks (lab only)"; }
    // Scan itself is passive, but this module hosts active actions, so gate it.
    bool        requiresAuthorization() const override { return true; }

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;
    void exit(Context& ctx) override;

private:
    struct Ap { String ssid; uint8_t bssid[6]; int32_t rssi; uint8_t ch; uint8_t enc; };
    void doScan(Context& ctx);

    // --- Active-action hooks (authorized lab use) -------------------------
    // Intentionally unimplemented. Fill in against your own AP/clients only.
    // Reference open implementations (Bruce / ESP32-Marauder) for the frame
    // formats; keep each action logged via ctx.log->tx(...).
    void captureHandshake(Context& ctx, const Ap& target);   // TODO: EAPOL/PMKID sniff
    void startEvilPortal(Context& ctx);                      // TODO: softAP + captive form
    void deauth(Context& ctx, const Ap& target);             // TODO: 802.11 deauth (lab)

    std::vector<Ap> aps_;
    int  sel_ = 0;
    bool scanning_ = false;
};
