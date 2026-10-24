// Rfid.h — RFID/NFC scaffold. The Cardputer ADV has no onboard reader; attach a
// PN532 (I2C/SPI) or MFRC522 (SPI) per docs/HARDWARE.md. This module defines a
// reader-agnostic interface so either can be dropped in. Read/emulate/write are
// left as hooks to implement against whichever reader you wire up.
#pragma once
#include "../../core/Module.h"

class RfidModule : public Module {
public:
    const char* name() const override { return "RFID/NFC"; }
    Category    category() const override { return Category::RFID; }
    const char* description() const override { return "Read/emulate tags (needs reader)"; }
    bool        requiresAuthorization() const override { return true; }

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;

private:
    bool readerPresent_ = false;
    void readTag(Context& ctx);   // TODO: UID + sector dump to SD
    void emulate(Context& ctx);   // TODO: card emulation (reader-dependent)
};
