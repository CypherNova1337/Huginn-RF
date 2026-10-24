#include "Wifi.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/Logger.h"
#include <WiFi.h>

void WifiModule::enter(Context& ctx) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(100);
    aps_.clear(); sel_ = 0;
    doScan(ctx);
}

void WifiModule::doScan(Context& ctx) {
    scanning_ = true;
    ctx.display->clear();
    ctx.display->header("WiFi Scan");
    ctx.display->line(0, "Scanning...", 0xFFE0);

    int n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);
    aps_.clear();
    for (int i = 0; i < n; ++i) {
        Ap ap;
        ap.ssid = WiFi.SSID(i);
        memcpy(ap.bssid, WiFi.BSSID(i), 6);
        ap.rssi = WiFi.RSSI(i);
        ap.ch   = WiFi.channel(i);
        ap.enc  = (uint8_t)WiFi.encryptionType(i);
        aps_.push_back(ap);
    }
    WiFi.scanDelete();
    scanning_ = false;
    if (ctx.log) ctx.log->info("WiFi scan: %d APs", (int)aps_.size());
}

bool WifiModule::loop(Context& ctx) {
    if (aps_.empty()) {
        ctx.display->line(0, "No APs. R=rescan", 0x8410);
        ctx.display->footer("Esc=back");
        KeyEvent e = ctx.input->poll();
        if (e.key == Key::Back) return false;
        if (e.key == Key::Char && (e.ch=='r'||e.ch=='R')) doScan(ctx);
        delay(20); return true;
    }

    const Ap& a = aps_[sel_];
    ctx.display->clear();
    ctx.display->header("WiFi Scan");
    ctx.display->printf(0, 0xFFFF, "%s", a.ssid.length()? a.ssid.c_str() : "<hidden>");
    ctx.display->printf(1, 0x8410, "%02X:%02X:%02X:%02X:%02X:%02X",
        a.bssid[0],a.bssid[1],a.bssid[2],a.bssid[3],a.bssid[4],a.bssid[5]);
    ctx.display->printf(2, 0x07E0, "ch %u  %ddBm  enc=%u", a.ch, (int)a.rssi, a.enc);
    ctx.display->printf(3, 0x8410, "%d/%d", sel_+1, (int)aps_.size());
    ctx.display->line(5, "R=rescan  (audit=TODO)", 0x8410);
    ctx.display->footer("Up/Down  Esc=back");

    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   sel_ = (sel_ + aps_.size() - 1) % aps_.size(); break;
        case Key::Down: sel_ = (sel_ + 1) % aps_.size(); break;
        case Key::Back: return false;
        case Key::Char: if (e.ch=='r'||e.ch=='R') doScan(ctx); break;
        default: break;
    }
    delay(10);
    return true;
}

void WifiModule::exit(Context& ctx) {
    WiFi.mode(WIFI_OFF);
}

// --- Active hooks: NOT implemented. Authorized lab use only. ---------------
void WifiModule::captureHandshake(Context& ctx, const Ap& target) {
    // TODO: switch to promiscuous mode on target.ch, capture EAPOL / PMKID to
    // an SD .pcap, log each via ctx.log->tx("wifi", ...). Your own AP only.
    (void)ctx; (void)target;
}
void WifiModule::startEvilPortal(Context& ctx) {
    // TODO: softAP + DNS catch-all + captive login form served from SD.
    // For authorized social-engineering assessments with written scope only.
    (void)ctx;
}
void WifiModule::deauth(Context& ctx, const Ap& target) {
    // TODO: craft/send 802.11 deauth frames. Jamming/deauth is illegal against
    // networks you don't control — keep this to an isolated lab.
    (void)ctx; (void)target;
}

REGISTER_MODULE(WifiModule);
