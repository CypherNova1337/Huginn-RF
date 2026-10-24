// Ir.h — IR transmitter. Loads raw codes from Flipper-style .ir files on the SD
// card and blasts them via the built-in emitter (TX only on stock hardware).
// Doubles as a TV-B-Gone by cycling through every code in the file.
#pragma once
#include "../../core/Module.h"
#include <vector>

class IrModule : public Module {
public:
    const char* name() const override { return "IR Transmit"; }
    Category    category() const override { return Category::IR; }
    const char* description() const override { return "Blast saved / TV-B-Gone IR codes"; }
    bool        requiresAuthorization() const override { return true; } // emits

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;
    void exit(Context& ctx) override;

private:
    struct Code {
        String name;
        uint16_t freq = 38000;
        std::vector<uint16_t> timings;   // alternating mark/space microseconds
    };

    void loadFile(Context& ctx, const char* path);
    void sendCode(Context& ctx, const Code& c);
    void sendRaw(const uint16_t* timings, size_t n, uint16_t freq);

    std::vector<Code> codes_;
    int  sel_ = 0;
    bool blasting_ = false;
    size_t blastIdx_ = 0;
    uint32_t lastBlastMs_ = 0;
};
