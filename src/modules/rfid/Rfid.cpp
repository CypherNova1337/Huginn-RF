#include "Rfid.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"

void RfidModule::enter(Context& ctx) {
    ctx.display->clear();
    ctx.display->header("RFID/NFC");
    // TODO: probe I2C (PN532) / SPI (MFRC522) here and set readerPresent_.
    readerPresent_ = false;
}

bool RfidModule::loop(Context& ctx) {
    if (!readerPresent_) {
        ctx.display->line(0, "No reader detected.", 0xF800);
        ctx.display->line(1, "Attach PN532/MFRC522", 0xFFFF);
        ctx.display->line(2, "per docs/HARDWARE.md", 0x8410);
    } else {
        ctx.display->line(0, "Reader ready.", 0x07E0);
        ctx.display->line(1, "Read/Emulate: TODO", 0xFFE0);
    }
    ctx.display->footer("Esc=back");
    if (ctx.input->poll().key == Key::Back) return false;
    delay(20);
    return true;
}

void RfidModule::readTag(Context& ctx) { (void)ctx; /* TODO */ }
void RfidModule::emulate(Context& ctx) { (void)ctx; /* TODO */ }

REGISTER_MODULE(RfidModule);
