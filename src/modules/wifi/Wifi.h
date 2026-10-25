// Wifi.h — 2.4 GHz WiFi auditing on the ESP32-S3 radio.
//
// Submenu:
//   Scan          passive AP scan (SSID/BSSID/channel/RSSI/encryption)
//   Sniffer       promiscuous frame counter, optional raw .pcap to SD
//   Handshake     passive EAPOL/PMKID capture on a channel -> .pcap on SD
//   Evil Portal   softAP + captive portal serving an operator template from SD
//
// All of the above are for authorized assessments only and are gated behind the
// authorized-use acknowledgement. Active deauth/jamming is intentionally left
// as a documented, gated hook (see Wifi.cpp) rather than a turnkey flood.
#pragma once
#include "../../core/Module.h"
#include <vector>

class WifiModule : public Module {
public:
    const char* name() const override { return "WiFi"; }
    Category    category() const override { return Category::WiFi; }
    const char* description() const override { return "Scan / sniff / capture / portal"; }
    bool        requiresAuthorization() const override { return true; }

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;
    void exit(Context& ctx) override;

private:
    struct Ap { String ssid; uint8_t bssid[6]; int32_t rssi; uint8_t ch; uint8_t enc; };

    enum class View { Menu, Scan, Sniffer, Handshake, Deauth, Portal, Export, Probe };
    View view_ = View::Menu;
    int  menuSel_ = 0;

    // --- Scan ---
    std::vector<Ap> aps_;
    int  scanSel_ = 0;
    void doScan(Context& ctx);
    bool loopScan(Context& ctx);

    // --- Sniffer / Handshake (both use promiscuous mode) ---
    uint8_t  channel_ = 1;
    bool     startPromiscuous(Context& ctx, bool eapolOnly);
    void     stopPromiscuous();
    bool     loopSniffer(Context& ctx, bool eapolOnly);

    // --- Targeted deauth (authorized reauth-capture; single scoped BSSID) ---
    int      deauthTarget_ = -1;   // index into aps_
    uint32_t deauthSent_ = 0;
    bool     loopDeauth(Context& ctx);
    void     sendDeauthBurst(Context& ctx, const Ap& ap);

    // --- Evil Portal ---
    bool startPortal(Context& ctx);
    bool loopPortal(Context& ctx);
    void stopPortal();

    // --- Hashcat export: captured EAPOL/PMKID pcap -> .hc22000 ---
    bool loopExport(Context& ctx);
    int  exportHandshakes(Context& ctx);   // returns lines written, -1 on error

    // --- Probe-request SSID harvest (which networks nearby devices seek) ---
    void startProbe(Context& ctx);
    void stopProbe(Context& ctx);
    bool loopProbe(Context& ctx);
    int  probeSel_ = 0;

    uint32_t lastUiMs_ = 0;
};
