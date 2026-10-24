#include "Rfid.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/SdCard.h"
#include "../../core/Logger.h"
#include "../../hal/board.h"

#ifdef HUGINN_HAS_RFID
#include <Wire.h>
#include <Adafruit_PN532.h>

static Adafruit_PN532 s_nfc(RFID_PN532_IRQ, RFID_PN532_RESET);

bool RfidModule::initReader(Context& ctx) {
    Wire.begin(RFID_I2C_SDA, RFID_I2C_SCL);
    s_nfc.begin();
    uint32_t ver = s_nfc.getFirmwareVersion();
    if (!ver) return false;
    s_nfc.SAMConfig();
    if (ctx.log) ctx.log->info("PN532 fw 0x%08lX", (unsigned long)ver);
    return true;
}

void RfidModule::readTag(Context& ctx) {
    uint8_t uid[7] = {0};
    uint8_t uidLen = 0;
    if (!s_nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 150))
        return;

    char hex[21] = {0};
    for (uint8_t i = 0; i < uidLen && i < 7; ++i) snprintf(hex + i*2, 3, "%02X", uid[i]);
    lastUid_ = String(hex);

    // Append the UID (and a default-key sector-0 probe) to an SD log.
    File f = ctx.sd ? ctx.sd->openCapture("rfid", "log") : File();
    if (f) {
        f.printf("[%lu] UID=%s len=%u\n", (unsigned long)millis(), hex, uidLen);
        uint8_t key[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};  // Mifare default key A
        if (uidLen == 4 && s_nfc.mifareclassic_AuthenticateBlock(uid, uidLen, 0, 0, key)) {
            uint8_t blk[16];
            if (s_nfc.mifareclassic_ReadDataBlock(0, blk)) {
                f.print("  blk0:");
                for (int i = 0; i < 16; ++i) f.printf(" %02X", blk[i]);
                f.println();
            }
        }
        f.close();
    }
    if (ctx.log) ctx.log->info("RFID UID %s", hex);
}
#endif // HUGINN_HAS_RFID

void RfidModule::enter(Context& ctx) {
    ctx.display->clear(); ctx.display->header("RFID/NFC");
#ifdef HUGINN_HAS_RFID
    readerPresent_ = initReader(ctx);
#else
    readerPresent_ = false;
#endif
}

bool RfidModule::loop(Context& ctx) {
#ifndef HUGINN_HAS_RFID
    ctx.display->line(0, "RFID not in this build.", 0xF800);
    ctx.display->line(1, "Attach PN532 on Grove I2C", 0xFFFF);
    ctx.display->line(2, "and flash cardputer_adv_full", 0x8410);
    ctx.display->footer("Esc=back");
    if (ctx.input->poll().key == Key::Back) return false;
    delay(20);
    return true;
#else
    if (!readerPresent_) {
        ctx.display->line(0, "PN532 not detected.", 0xF800);
        ctx.display->line(1, "Check Grove I2C wiring.", 0x8410);
        ctx.display->footer("Esc=back");
        if (ctx.input->poll().key == Key::Back) return false;
        delay(20); return true;
    }

    if (millis() - lastReadMs_ > 300) { lastReadMs_ = millis(); readTag(ctx); }

    ctx.display->clear(); ctx.display->header("RFID/NFC");
    ctx.display->line(0, "Present a tag...", 0x07E0);
    ctx.display->printf(1, 0xFFFF, "UID: %s", lastUid_.length()? lastUid_.c_str() : "-");
    ctx.display->line(3, "Logged to SD captures/", 0x8410);
    ctx.display->footer("Esc=back");
    if (ctx.input->poll().key == Key::Back) return false;
    delay(10);
    return true;
#endif
}

REGISTER_MODULE(RfidModule);
