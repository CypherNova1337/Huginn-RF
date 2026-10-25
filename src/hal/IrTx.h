// IrTx.h — IR transmit helper: a LEDC-carrier raw sender plus encoders for the
// common consumer protocols, so you can "type a code" instead of only replaying
// captured raw timings. Shared by the IR module and the script engine.
#pragma once
#include <Arduino.h>
#include <vector>

namespace irtx {

// Drive the built-in emitter: alternating mark/space durations (microseconds),
// index 0 is a mark (carrier on). `freq` is the carrier in Hz (e.g. 38000).
void sendRaw(const uint16_t* timings, size_t n, uint16_t freq);
inline void sendRaw(const std::vector<uint16_t>& t, uint16_t freq) {
    sendRaw(t.data(), t.size(), freq);
}

// Pulse-distance / pulse-width encoders. Each returns a mark/space buffer to
// hand to sendRaw with the paired carrier frequency.
std::vector<uint16_t> encodeNEC(uint8_t addr, uint8_t cmd);          // 38 kHz
std::vector<uint16_t> encodeNECext(uint16_t addr16, uint8_t cmd);    // 38 kHz
std::vector<uint16_t> encodeSamsung(uint8_t addr, uint8_t cmd);      // 38 kHz
std::vector<uint16_t> encodeSony12(uint8_t cmd7, uint8_t addr5);     // 40 kHz
std::vector<uint16_t> encodeSony20(uint8_t cmd7, uint16_t addr13);   // 40 kHz
std::vector<uint16_t> encodeRC5(uint8_t addr5, uint8_t cmd6);        // 36 kHz (Manchester)

} // namespace irtx
