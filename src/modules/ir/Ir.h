// Ir.h — IR transmitter. Two modes:
//   From file   load Flipper-style raw .ir codes from the SD card and blast one
//               or cycle all (TV-B-Gone style).
//   Protocol    "type a code": pick NEC / NEC-ext / Samsung / Sony, enter the
//               address + command in hex, and it encodes and sends.
// TX only on stock hardware (no onboard IR receiver).
#pragma once
#include "../../core/Module.h"
#include <vector>

class IrModule : public Module {
public:
    const char* name() const override { return "IR Transmit"; }
    Category    category() const override { return Category::IR; }
    const char* description() const override { return "Blast .ir files or typed codes"; }
    bool        requiresAuthorization() const override { return true; } // emits

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;
    void exit(Context& ctx) override;

private:
    struct Code {
        String name;
        uint16_t freq = 38000;
        std::vector<uint16_t> timings;
    };

    enum class View { Menu, File, Proto };
    View view_ = View::Menu;
    int  menuSel_ = 0;

    void loadFile(Context& ctx, const char* path);
    void sendCode(Context& ctx, const Code& c);
    bool loopFile(Context& ctx);
    bool loopProto(Context& ctx);

    std::vector<Code> codes_;
    int  sel_ = 0;
    bool blasting_ = false;
    size_t blastIdx_ = 0;
    uint32_t lastBlastMs_ = 0;
    int  protoSel_ = 0;
};
