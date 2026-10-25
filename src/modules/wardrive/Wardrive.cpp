#include "Wardrive.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/SdCard.h"
#include "../../core/Config.h"
#include "../../core/Logger.h"
#include "../../hal/Lora.h"
#include "../../hal/Gps.h"
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

static String authString(wifi_auth_mode_t m) {
    switch (m) {
        case WIFI_AUTH_OPEN:            return "[OPEN]";
        case WIFI_AUTH_WEP:             return "[WEP]";
        case WIFI_AUTH_WPA_PSK:         return "[WPA-PSK-TKIP][ESS]";
        case WIFI_AUTH_WPA2_PSK:        return "[WPA2-PSK-CCMP][ESS]";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "[WPA-WPA2-PSK][ESS]";
        case WIFI_AUTH_WPA3_PSK:        return "[WPA3-SAE][ESS]";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "[WPA2-WPA3-PSK][ESS]";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "[WPA2-EAP][ESS]";
        default:                        return "[ESS]";
    }
}

void WardriveModule::enter(Context& ctx) {
    minRssi_  = ctx.config->getInt("wardrive.min_rssi", -95);
    dedupSec_ = ctx.config->getInt("wardrive.dedup_seconds", 30);
    doWifi_   = ctx.config->getBool("wardrive.log_wifi", true);
    doBle_    = ctx.config->getBool("wardrive.log_ble", true);
    doLora_   = ctx.config->getBool("wardrive.log_lora", false);
    wifiCount_ = bleCount_ = loraCount_ = 0;
    phase_ = 0;
    lastSeen_.clear();

    String prefix = ctx.config->getString("wardrive.csv_prefix", "wardrive");
    csv_ = ctx.sd ? ctx.sd->openCapture(prefix.c_str(), "csv") : File();
    writeHeader();

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    if (doBle_) BLEDevice::init("");
    if (doLora_ && ctx.lora) ctx.lora->startReceive();

    ctx.display->clear();
    ctx.display->header("Wardrive");
}

void WardriveModule::writeHeader() {
    if (!csv_) return;
    csv_.println("WigleWifi-1.6,appRelease=0.2.0,model=CardputerADV,"
                 "release=HuginnRF,device=huginn,display=,board=esp32s3,brand=M5Stack");
    csv_.println("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,"
                 "CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type");
    csv_.flush();
}

bool WardriveModule::fresh(const String& key, uint32_t now) {
    auto it = lastSeen_.find(key);
    bool f = (it == lastSeen_.end()) || (now - it->second) > dedupSec_ * 1000UL;
    lastSeen_[key] = now;
    return f;
}

void WardriveModule::logRow(Context& ctx, const char* mac, const String& ssid,
                            const String& auth, int rssi, int channel,
                            const char* type) {
    if (!csv_ || !ctx.gps->hasFix()) return;
    auto fix = ctx.gps->fix();
    // SSID may contain commas; wrap in quotes.
    csv_.printf("%s,\"%s\",%s,%s,%d,%d,%.6f,%.6f,%.1f,%.1f,%s\n",
                mac, ssid.c_str(), auth.c_str(), ctx.gps->isoTime().c_str(),
                channel, rssi, fix.lat, fix.lon, fix.altMeters, 10.0, type);
    csv_.flush();
}

void WardriveModule::scanWifi(Context& ctx) {
    int n = WiFi.scanNetworks(false, true);
    uint32_t now = millis();
    for (int i = 0; i < n; ++i) {
        if (WiFi.RSSI(i) < minRssi_) continue;
        String bssid = WiFi.BSSIDstr(i);
        if (!fresh(String("W:") + bssid, now)) continue;
        logRow(ctx, bssid.c_str(), WiFi.SSID(i),
               authString((wifi_auth_mode_t)WiFi.encryptionType(i)),
               WiFi.RSSI(i), WiFi.channel(i), "WIFI");
        wifiCount_++;
    }
    WiFi.scanDelete();
}

void WardriveModule::scanBle(Context& ctx) {
    BLEScan* scan = BLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(100); scan->setWindow(99);
    BLEScanResults res = scan->start(3, false);
    uint32_t now = millis();
    for (int i = 0; i < res.getCount(); ++i) {
        BLEAdvertisedDevice d = res.getDevice(i);
        int rssi = d.haveRSSI() ? d.getRSSI() : -100;
        if (rssi < minRssi_) continue;
        String addr = String(d.getAddress().toString().c_str());
        if (!fresh(String("B:") + addr, now)) continue;
        String name = d.haveName() ? String(d.getName().c_str()) : String("");
        logRow(ctx, addr.c_str(), name, "[BLE]", rssi, 0, "BLE");
        bleCount_++;
    }
    scan->clearResults();
}

void WardriveModule::pollLora(Context& ctx) {
    if (!ctx.lora) return;
    Lora::RxPacket pkt;
    // Drain a few packets per visit so a busy channel doesn't starve the UI.
    for (int k = 0; k < 4 && ctx.lora->receive(pkt); ++k) {
        if (pkt.rssi < minRssi_) continue;
        uint32_t h = 2166136261u;
        for (size_t i = 0; i < pkt.len; ++i) { h ^= pkt.data[i]; h *= 16777619u; }
        char mac[18];
        snprintf(mac, sizeof(mac), "02:%02X:%02X:%02X:%02X:%02X",
                 (h>>24)&0xFF,(h>>16)&0xFF,(h>>8)&0xFF,h&0xFF,(unsigned)(pkt.len&0xFF));
        if (!fresh(String("L:") + mac, millis())) continue;
        logRow(ctx, mac, "LoRa", "[LORA]", (int)pkt.rssi, 0, "LoRa");
        loraCount_++;
    }
}

bool WardriveModule::loop(Context& ctx) {
    ctx.gps->update();

    // One radio action per loop pass, round-robin, so the UI stays responsive.
    if (doLora_) pollLora(ctx);
    switch (phase_) {
        case 0: if (doWifi_) scanWifi(ctx); break;
        case 1: if (doBle_)  scanBle(ctx);  break;
    }
    phase_ = (phase_ + 1) % 2;

    if (millis() - lastUiMs_ > 250) {
        lastUiMs_ = millis();
        auto fix = ctx.gps->fix();
        ctx.display->clear(); ctx.display->header("Wardrive");
        ctx.display->printf(0, 0xFFFF, "WiFi:%lu BLE:%lu LoRa:%lu",
            (unsigned long)wifiCount_, (unsigned long)bleCount_, (unsigned long)loraCount_);
        ctx.display->printf(1, ctx.gps->hasFix()?0x07E0:0xF800,
            "GPS: %s  sats=%u", ctx.gps->hasFix()?"fix":"NO FIX (not logging)", fix.satellites);
        if (ctx.gps->hasFix())
            ctx.display->printf(2, 0x8410, "%.5f, %.5f", fix.lat, fix.lon);
        ctx.display->printf(3, 0x8410, "unique seen: %u", (unsigned)lastSeen_.size());
        ctx.display->footer("Esc=stop & save");
    }

    if (ctx.input->poll().key == Key::Back) return false;
    return true;
}

void WardriveModule::exit(Context& ctx) {
    if (csv_) { csv_.flush(); csv_.close(); }
    if (doBle_) BLEDevice::deinit(true);
    WiFi.mode(WIFI_OFF);
    if (ctx.log) ctx.log->info("Wardrive stop: WiFi=%lu BLE=%lu LoRa=%lu",
        (unsigned long)wifiCount_, (unsigned long)bleCount_, (unsigned long)loraCount_);
}

REGISTER_MODULE(WardriveModule);
