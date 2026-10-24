// Rfid.h — 13.56 MHz RFID/NFC via a PN532 on the Grove I2C port. Built with the
// cardputer_adv_full env (HUGINN_HAS_RFID) and an attached PN532. Reads tag UIDs
// and dumps Mifare Classic sectors with default keys to the SD card. Without the
// hardware/build it shows a notice. (Write/clone needs magic cards + known keys
// and is a bounded follow-up.)
#pragma once
#include "../../core/Module.h"

class RfidModule : public Module {
public:
    const char* name() const override { return "RFID/NFC"; }
    Category    category() const override { return Category::RFID; }
    const char* description() const override { return "PN532 read / dump tags"; }
    bool        requiresAuthorization() const override { return true; }

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;

private:
    bool readerPresent_ = false;
#ifdef HUGINN_HAS_RFID
    bool initReader(Context& ctx);
    void readTag(Context& ctx);
    uint32_t lastReadMs_ = 0;
    String   lastUid_;
#endif
};
