#include "Recon.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/Config.h"
#include "../../core/Logger.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// --- Shared device table (written by the promiscuous cb + BLE scan) --------
struct Seen {
    uint8_t  mac[6];
    int8_t   rssi;
    uint32_t lastMs;
    uint8_t  kind;   // 0 = WiFi, 1 = BLE
};
static const int kMax = 256;
static Seen g_tbl[kMax];
static volatile int g_count = 0;

static void upsert(const uint8_t* mac, int8_t rssi, uint8_t kind) {
    for (int i = 0; i < g_count; ++i) {
        if (memcmp(g_tbl[i].mac, mac, 6) == 0) {
            g_tbl[i].rssi = rssi; g_tbl[i].lastMs = millis(); g_tbl[i].kind = kind;
            return;
        }
    }
    if (g_count < kMax) {
        memcpy(g_tbl[g_count].mac, mac, 6);
        g_tbl[g_count].rssi = rssi; g_tbl[g_count].lastMs = millis();
        g_tbl[g_count].kind = kind; g_count++;
    }
}

// WiFi task context (not an ISR): pull transmitter MAC (addr2) + RSSI.
static void promiscuousCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    auto* p = (wifi_promiscuous_pkt_t*)buf;
    int len = p->rx_ctrl.sig_len;
    if (len < 16) return;                      // need the 802.11 header + addr2
    upsert(p->payload + 10, (int8_t)p->rx_ctrl.rssi, 0);
}

void ReconModule::startCapture(Context& ctx) {
    g_count = 0;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&promiscuousCb);
    esp_wifi_set_channel(channel_, WIFI_SECOND_CHAN_NONE);
    BLEDevice::init("");
}

void ReconModule::stopCapture() {
    esp_wifi_set_promiscuous(false);
    BLEDevice::deinit(true);
    WiFi.mode(WIFI_OFF);
}

void ReconModule::maybeBleScan(Context& ctx) {
    if (millis() - lastBleMs_ < bleEveryMs_) return;
    lastBleMs_ = millis();
    // Keep the screen alive during the (brief, blocking) BLE sweep.
    ctx.display->footer("+ BLE scan...");
    ctx.display->present();
    BLEScan* scan = BLEDevice::getScan();
    scan->setActiveScan(true); scan->setInterval(80); scan->setWindow(60);
    BLEScanResults res = scan->start(1, false);   // 1s keeps WiFi recon responsive
    for (int i = 0; i < res.getCount(); ++i) {
        BLEAdvertisedDevice d = res.getDevice(i);
        int rssi = d.haveRSSI() ? d.getRSSI() : -100;
        if (rssi < minRssi_) continue;
        upsert(*d.getAddress().getNative(), (int8_t)rssi, 1);
    }
    scan->clearResults();
}

void ReconModule::enter(Context& ctx) {
    forgetMs_   = (uint32_t)ctx.config->getInt("recon.forget_seconds", 60) * 1000UL;
    bleEveryMs_ = (uint32_t)ctx.config->getInt("recon.ble_every_seconds", 6) * 1000UL;
    minRssi_    = ctx.config->getInt("recon.min_rssi", -95);
    view_ = View::List; sel_ = 0;
    startCapture(ctx);
}

void ReconModule::exit(Context& ctx) {
    stopCapture();
}

// Build a display order: fresh entries, strongest first. Returns count, fills idx[].
static int buildOrder(int* idx, int cap, uint32_t forgetMs, int minRssi) {
    uint32_t now = millis();
    int m = 0;
    for (int i = 0; i < g_count && m < cap; ++i) {
        if (g_tbl[i].rssi < minRssi) continue;
        if (now - g_tbl[i].lastMs > forgetMs) continue;
        idx[m++] = i;
    }
    // insertion sort by rssi desc (small N)
    for (int a = 1; a < m; ++a) {
        int key = idx[a], b = a - 1;
        while (b >= 0 && g_tbl[idx[b]].rssi < g_tbl[key].rssi) { idx[b+1] = idx[b]; b--; }
        idx[b+1] = key;
    }
    return m;
}

bool ReconModule::loop(Context& ctx) {
    // Hop WiFi channels so we see devices on all of them.
    if (millis() - lastHopMs_ > 250) {
        lastHopMs_ = millis();
        channel_ = (channel_ % 13) + 1;
        esp_wifi_set_channel(channel_, WIFI_SECOND_CHAN_NONE);
    }
    maybeBleScan(ctx);

    return (view_ == View::Track) ? loopTrack(ctx) : loopList(ctx);
}

bool ReconModule::loopList(Context& ctx) {
    int idx[64];
    int m = buildOrder(idx, 64, forgetMs_, minRssi_);
    if (sel_ >= m) sel_ = m > 0 ? m - 1 : 0;

    if (millis() - lastUiMs_ > 300) {
        lastUiMs_ = millis();
        ctx.display->clear();
        ctx.display->header("Who's Around");
        ctx.display->printf(0, 0xFFFF, "Nearby now: %d  (ch%u)", m, channel_);
        int rows = ctx.display->bodyRows() - 2;
        int first = sel_ >= rows ? sel_ - rows + 1 : 0;
        for (int r = 0; r < rows && (first + r) < m; ++r) {
            int e = idx[first + r];
            bool s = (first + r == sel_);
            // Signal bar: map RSSI (-90..-40) -> 0..5 blocks. Closer = fuller.
            int rssi = g_tbl[e].rssi;
            int clamp = rssi < -90 ? -90 : (rssi > -40 ? -40 : rssi);
            int bars = (clamp + 90) * 5 / 50;
            char bar[6]; int bi = 0;
            for (; bi < bars && bi < 5; ++bi) bar[bi] = '|';
            for (; bi < 5; ++bi) bar[bi] = '.';
            bar[5] = 0;
            char line[48];
            snprintf(line, sizeof(line), "%s%s %s %02X%02X%02X %d",
                     s ? ">" : " ", bar, g_tbl[e].kind ? "BLE" : "WiF",
                     g_tbl[e].mac[3], g_tbl[e].mac[4], g_tbl[e].mac[5], rssi);
            // Colour by proximity: near=green, mid=white, far=grey.
            uint16_t col = s ? 0xFFE0 : (rssi > -55 ? 0x07E0 : (rssi > -75 ? 0xFFFF : 0x8410));
            ctx.display->line(1 + r, line, col);
        }
        ctx.display->footer("Up/Dn  Enter=track  Esc=back");
    }

    KeyEvent ev = ctx.input->poll();
    switch (ev.key) {
        case Key::Up:   if (sel_ > 0) sel_--; break;
        case Key::Down: if (sel_ < m - 1) sel_++; break;
        case Key::Enter:
            if (m > 0) {
                memcpy(target_, g_tbl[idx[sel_]].mac, 6);
                trackPeak_ = -120;
                view_ = View::Track;
                if (ctx.log) ctx.log->info("Track lock %02X:%02X:%02X:%02X:%02X:%02X",
                    target_[0],target_[1],target_[2],target_[3],target_[4],target_[5]);
            }
            break;
        case Key::Back: return false;
        default: break;
    }
    delay(5);
    return true;
}

bool ReconModule::loopTrack(Context& ctx) {
    // Find the target's current reading.
    int rssi = -120; uint32_t age = 999999; uint8_t kind = 0;
    for (int i = 0; i < g_count; ++i) {
        if (memcmp(g_tbl[i].mac, target_, 6) == 0) {
            rssi = g_tbl[i].rssi; age = millis() - g_tbl[i].lastMs; kind = g_tbl[i].kind; break;
        }
    }
    if (rssi > trackPeak_) trackPeak_ = rssi;

    if (millis() - lastUiMs_ > 150) {
        lastUiMs_ = millis();
        ctx.display->clear();
        ctx.display->header("Track device");
        ctx.display->printf(0, 0xFFFF, "%s %02X:%02X:%02X:%02X:%02X:%02X",
            kind ? "BLE" : "WiFi",
            target_[0],target_[1],target_[2],target_[3],target_[4],target_[5]);

        bool stale = (age > 4000);
        // Map RSSI (-95..-30) to a 0..20 bar.
        int clamped = rssi < -95 ? -95 : (rssi > -30 ? -30 : rssi);
        int bars = ((clamped + 95) * 20) / 65;
        char meter[24]; int k = 0;
        for (; k < bars && k < 20; ++k) meter[k] = '#';
        for (; k < 20; ++k) meter[k] = '-';
        meter[20] = 0;

        ctx.display->printf(1, stale ? 0xF800 : 0x07E0,
            "%s %ddBm", stale ? "(lost)" : "RSSI", rssi);
        ctx.display->line(2, meter, stale ? 0x8410 : 0x07E0);
        ctx.display->printf(3, 0x8410, "peak %ddBm  closer = more #", trackPeak_);
        ctx.display->line(4, "Walk around to home in.", 0x8410);
        ctx.display->footer("Esc=back to list");
    }

    if (ctx.input->poll().key == Key::Back) { view_ = View::List; }
    delay(5);
    return true;
}

REGISTER_MODULE(ReconModule);
