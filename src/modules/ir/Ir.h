// Ir.h — IR transmitter.
//   Browse library   list every .ir file in /huginn/ir (drop a whole Flipper
//                     library organised by brand), open one, send/blast codes.
//   Send protocol     "type a code": NEC / NEC-ext / Samsung / Sony-12 /
//                     Sony-20 / RC5, plus a NEC command sweep (brute).
//   Smart power       fire a built-in set of common TV power codes across
//                     brands, most-common first (TV-B-Gone style).
//   Save last code    append the last code you sent to a named .ir on SD, so
//                     you can build your own remote on-device.
// TX only on stock hardware.
#pragma once
#include "../../core/Module.h"
#include <vector>

class IrModule : public Module {
public:
    const char* name() const override { return "IR Transmit"; }
    Category    category() const override { return Category::IR; }
    const char* description() const override { return "Library / codes / smart power"; }
    bool        requiresAuthorization() const override { return true; }

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;
    void exit(Context& ctx) override;

private:
    struct Code { String name; uint16_t freq = 38000; std::vector<uint16_t> timings; };

    enum class View { Menu, Library, FileCodes, Proto };
    View view_ = View::Menu;
    int  menuSel_ = 0;

    void loadLibrary(Context& ctx);
    void loadFile(Context& ctx, const char* path);
    void sendCode(Context& ctx, const Code& c);
    void smartPowerBlast(Context& ctx);
    void saveLast(Context& ctx);

    bool loopLibrary(Context& ctx);
    bool loopFileCodes(Context& ctx);
    bool loopProto(Context& ctx);

    std::vector<String> libFiles_;
    int  libSel_ = 0;

    std::vector<Code> codes_;        // codes in the currently open file
    int  sel_ = 0;
    bool blasting_ = false;
    size_t blastIdx_ = 0;
    uint32_t lastBlastMs_ = 0;

    int  protoSel_ = 0;
    Code last_;                      // last code sent (for "save")
};
