// Lora.h — RadioLib SX1262 wrapper. Provides LoRa TX/RX used by the messenger,
// wardrive listener, and the LoRaWAN join-fuzzer harness.
#pragma once
#include <Arduino.h>

class Lora {
public:
    struct RxPacket {
        uint8_t  data[256];
        size_t   len = 0;
        float    rssi = 0;
        float    snr = 0;
        uint32_t freqErrHz = 0;
    };

    // Returns false if the SX1262 isn't found/initialised (check wiring/board.h).
    bool begin(float freqMhz, float bwKhz, uint8_t sf, int8_t txPowerDbm);
    bool ready() const { return ready_; }

    bool setFrequency(float freqMhz);

    // Retune the whole modem for a different LoRa "preset" (used by the LoRa
    // activity scanner and the Meshtastic monitor). cr is the 4/x coding-rate
    // denominator (5..8). Returns true on success; leaves RX running.
    bool reconfigure(float freqMhz, float bwKhz, uint8_t sf, uint8_t cr,
                     uint8_t syncWord);

    // Blocking transmit of a raw buffer. Returns true on success.
    bool transmit(const uint8_t* data, size_t len);

    // Start continuous receive; poll with receive().
    void startReceive();
    // Non-blocking: returns true and fills `out` when a packet arrived.
    bool receive(RxPacket& out);

    // Instantaneous RSSI of the channel (for a crude activity/energy scan).
    float channelRssi();

private:
    bool  ready_ = false;
    float freqMhz_ = 915.0f;
};
