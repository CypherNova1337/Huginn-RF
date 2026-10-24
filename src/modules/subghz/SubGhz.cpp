#include "SubGhz.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"

void SubGhzModule::enter(Context& ctx) {
    ctx.display->clear();
    ctx.display->header("Sub-GHz OOK");
}

bool SubGhzModule::loop(Context& ctx) {
#ifdef HUGINN_HAS_CC1101
    ctx.display->line(0, "CC1101 build.", 0x07E0);
    ctx.display->line(1, "Capture/Replay: TODO", 0xFFE0);
    ctx.display->line(2, "Wire per HARDWARE.md", 0x8410);
#else
    ctx.display->line(0, "No CC1101 in this build.", 0xF800);
    ctx.display->line(1, "SX1262 LoRa cap can't do", 0xFFFF);
    ctx.display->line(2, "general OOK cloning well.", 0xFFFF);
    ctx.display->line(3, "Flash cardputer_adv_cc1101", 0x8410);
    ctx.display->line(4, "with a CC1101 attached.", 0x8410);
#endif
    ctx.display->footer("Esc=back");
    if (ctx.input->poll().key == Key::Back) return false;
    delay(20);
    return true;
}

void SubGhzModule::captureRaw(Context& ctx) { (void)ctx; /* TODO */ }
void SubGhzModule::replayRaw(Context& ctx)  { (void)ctx; /* TODO */ }

REGISTER_MODULE(SubGhzModule);
