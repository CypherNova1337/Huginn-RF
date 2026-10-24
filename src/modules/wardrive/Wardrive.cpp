#include "Wardrive.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/SdCard.h"
#include "../../core/Config.h"
#include "../../core/Logger.h"
#include "../../hal/Lora.h"
#include "../../hal/Gps.h"

void WardriveModule::enter(Context& ctx) {
    minRssi_  = ctx.config->getInt("wardrive.min_rssi", -120);
    dedupSec_ = ctx.config->getInt("wardrive.dedup_seconds", 30);
    seen_ = 0;
    lastSeenMs_.clear();

    String prefix = ctx.config->getString("wardrive.csv_prefix", "wardrive");
    csv_ = ctx.sd->openCapture(prefix.c_str(), "csv");
    writeHeader(ctx);

    if (ctx.lora) ctx.lora->startReceive();
    ctx.display->clear();
    ctx.display->header("LoRa Wardrive");
}

void WardriveModule::writeHeader(Context& ctx) {
    if (!csv_) return;
    // WiGLE pre-header + column header. Non-WiFi sources use Type=LoRa; WiGLE
    // keeps unknown types, and you can post-process for other ingest targets.
    csv_.println("WigleWifi-1.6,appRelease=0.1.0,model=CardputerADV,"
                 "release=HuginnRF,device=huginn,display=,board=esp32s3,brand=M5Stack");
    csv_.println("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,"
                 "CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type");
    csv_.flush();
}

// Derive a stable pseudo-identifier from the packet so repeat sightings of the
// same source dedup. Not a real MAC — a FNV-1a hash rendered as a MAC-like id.
uint32_t WardriveModule::packetId(const uint8_t* data, size_t len) const {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; ++i) { h ^= data[i]; h *= 16777619u; }
    return h;
}

bool WardriveModule::loop(Context& ctx) {
    ctx.gps->update();

    Lora::RxPacket pkt;
    if (ctx.lora && ctx.lora->receive(pkt)) {
        if (pkt.rssi >= minRssi_) {
            uint32_t id = packetId(pkt.data, pkt.len);
            uint32_t now = millis();
            auto it = lastSeenMs_.find(id);
            bool fresh = (it == lastSeenMs_.end()) ||
                         (now - it->second) > dedupSec_ * 1000UL;
            if (fresh && csv_ && ctx.gps->hasFix()) {
                auto fix = ctx.gps->fix();
                // MAC-like rendering of the hash + packet length nibble.
                char mac[18];
                snprintf(mac, sizeof(mac), "02:%02X:%02X:%02X:%02X:%02X",
                         (id >> 24) & 0xFF, (id >> 16) & 0xFF,
                         (id >> 8) & 0xFF, id & 0xFF, (unsigned)(pkt.len & 0xFF));
                csv_.printf("%s,%s,[LORA],%s,0,%d,%.6f,%.6f,%.1f,%.1f,LoRa\n",
                            mac, "LoRa-ping", ctx.gps->isoTime().c_str(),
                            (int)pkt.rssi, fix.lat, fix.lon, fix.altMeters, 10.0);
                csv_.flush();
                seen_++;
            }
            lastSeenMs_[id] = now;
        }
    }

    // Refresh the HUD a few times a second.
    if (millis() - lastUiMs_ > 400) {
        lastUiMs_ = millis();
        auto fix = ctx.gps->fix();
        ctx.display->printf(0, 0xFFFF, "Logged: %lu", (unsigned long)seen_);
        ctx.display->printf(1, 0x07E0, "GPS: %s sats=%u",
                            ctx.gps->hasFix() ? "fix" : "no-fix", fix.satellites);
        if (ctx.gps->hasFix())
            ctx.display->printf(2, 0x8410, "%.5f, %.5f", fix.lat, fix.lon);
        ctx.display->printf(3, 0x8410, "freq set in config.yaml");
        ctx.display->footer("Esc=stop & save");
    }

    KeyEvent e = ctx.input->poll();
    if (e.key == Key::Back) return false;
    return true;
}

void WardriveModule::exit(Context& ctx) {
    if (csv_) { csv_.flush(); csv_.close(); }
    if (ctx.log) ctx.log->info("Wardrive stopped, %lu rows", (unsigned long)seen_);
}

REGISTER_MODULE(WardriveModule);
