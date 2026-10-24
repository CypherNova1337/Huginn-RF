#include "IrTx.h"
#include "board.h"

namespace irtx {

static constexpr int kCh = 0;   // LEDC channel for the IR carrier

void sendRaw(const uint16_t* timings, size_t n, uint16_t freq) {
    ledcSetup(kCh, freq ? freq : 38000, 8);
    ledcAttachPin(IR_TX_PIN, kCh);
    for (size_t i = 0; i < n; ++i) {
        ledcWrite(kCh, (i % 2 == 0) ? 128 : 0);   // even index = mark (carrier on)
        delayMicroseconds(timings[i]);
    }
    ledcWrite(kCh, 0);
    ledcDetachPin(IR_TX_PIN);
}

// Append 8 bits of `b`, LSB first, as pulse-distance pairs (mark then space).
static void pushByteLSB(std::vector<uint16_t>& v, uint8_t b,
                        uint16_t mark, uint16_t zero, uint16_t one) {
    for (int i = 0; i < 8; ++i) {
        v.push_back(mark);
        v.push_back(((b >> i) & 1) ? one : zero);
    }
}

std::vector<uint16_t> encodeNEC(uint8_t addr, uint8_t cmd) {
    std::vector<uint16_t> v;
    v.push_back(9000); v.push_back(4500);
    pushByteLSB(v, addr,        560, 560, 1690);
    pushByteLSB(v, (uint8_t)~addr, 560, 560, 1690);
    pushByteLSB(v, cmd,         560, 560, 1690);
    pushByteLSB(v, (uint8_t)~cmd,  560, 560, 1690);
    v.push_back(560);           // final burst
    return v;
}

std::vector<uint16_t> encodeNECext(uint16_t addr16, uint8_t cmd) {
    std::vector<uint16_t> v;
    v.push_back(9000); v.push_back(4500);
    pushByteLSB(v, addr16 & 0xFF,        560, 560, 1690);
    pushByteLSB(v, (addr16 >> 8) & 0xFF, 560, 560, 1690);
    pushByteLSB(v, cmd,         560, 560, 1690);
    pushByteLSB(v, (uint8_t)~cmd,  560, 560, 1690);
    v.push_back(560);
    return v;
}

std::vector<uint16_t> encodeSamsung(uint8_t addr, uint8_t cmd) {
    std::vector<uint16_t> v;
    v.push_back(4500); v.push_back(4500);   // Samsung header
    pushByteLSB(v, addr,        560, 560, 1690);
    pushByteLSB(v, addr,        560, 560, 1690);
    pushByteLSB(v, cmd,         560, 560, 1690);
    pushByteLSB(v, (uint8_t)~cmd,  560, 560, 1690);
    v.push_back(560);
    return v;
}

// Sony SIRC 12-bit: 7 command bits then 5 address bits, LSB first.
// '1' = 1200us mark, '0' = 600us mark, each followed by a 600us space.
std::vector<uint16_t> encodeSony12(uint8_t cmd7, uint8_t addr5) {
    std::vector<uint16_t> v;
    v.push_back(2400); v.push_back(600);    // header
    for (int i = 0; i < 7; ++i) {
        v.push_back(((cmd7 >> i) & 1) ? 1200 : 600);
        v.push_back(600);
    }
    for (int i = 0; i < 5; ++i) {
        v.push_back(((addr5 >> i) & 1) ? 1200 : 600);
        v.push_back(600);
    }
    return v;
}

} // namespace irtx
