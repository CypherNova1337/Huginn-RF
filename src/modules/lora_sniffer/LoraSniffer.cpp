#include "LoraSniffer.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/SdCard.h"
#include "../../core/Logger.h"
#include "../../hal/Lora.h"
#include "../../hal/Gps.h"

void LoraSnifferModule::enter(Context& ctx) {
    count_ = 0; lastLine_ = "";
    log_ = ctx.sd ? ctx.sd->openCapture("lorarx", "log") : File();
    if (log_) log_.println("# ts_ms, rssi, snr, len, hex, lat, lon");
    if (ctx.lora) ctx.lora->startReceive();
    ctx.display->clear(); ctx.display->header("LoRa Sniffer");
}

bool LoraSnifferModule::loop(Context& ctx) {
    ctx.gps->update();

    Lora::RxPacket pkt;
    if (ctx.lora && ctx.lora->receive(pkt)) {
        count_++;
        // Render a short hex preview for the UI.
        char hex[49]; int n = (int)min(pkt.len, (size_t)24);
        for (int i = 0; i < n; ++i) snprintf(hex + i*2, 3, "%02X", pkt.data[i]);
        lastLine_ = String(hex);

        if (log_) {
            log_.printf("%lu,%.1f,%.2f,%u,", (unsigned long)millis(),
                        pkt.rssi, pkt.snr, (unsigned)pkt.len);
            for (size_t i = 0; i < pkt.len; ++i) log_.printf("%02X", pkt.data[i]);
            if (ctx.gps->hasFix()) {
                auto fx = ctx.gps->fix();
                log_.printf(",%.6f,%.6f", fx.lat, fx.lon);
            } else log_.print(",,");
            log_.println();
            log_.flush();
        }
    }

    if (millis() - lastUiMs_ > 300) {
        lastUiMs_ = millis();
        ctx.display->printf(0, 0xFFFF, "Packets: %lu", (unsigned long)count_);
        ctx.display->printf(1, 0x07E0, "RSSI %.0f SNR %.1f", pkt.rssi, pkt.snr);
        ctx.display->line(2, lastLine_.length()? lastLine_.c_str() : "(waiting)", 0x8410);
        ctx.display->footer("Esc=stop & save");
    }

    if (ctx.input->poll().key == Key::Back) return false;
    return true;
}

void LoraSnifferModule::exit(Context& ctx) {
    if (log_) { log_.flush(); log_.close(); }
    if (ctx.log) ctx.log->info("LoRa sniffer: %lu packets", (unsigned long)count_);
}

REGISTER_MODULE(LoraSnifferModule);
