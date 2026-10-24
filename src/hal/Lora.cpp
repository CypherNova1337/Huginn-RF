#include "Lora.h"
#include "board.h"
#include <RadioLib.h>
#include <SPI.h>

static SPIClass s_loraSpi(FSPI);
static SPISettings s_loraSpiSet(2000000, MSBFIRST, SPI_MODE0);
static SX1262 s_radio = new Module(LORA_PIN_NSS, LORA_PIN_DIO1, LORA_PIN_RST,
                                   LORA_PIN_BUSY, s_loraSpi, s_loraSpiSet);

static volatile bool s_rxFlag = false;
static void IRAM_ATTR onDio1() { s_rxFlag = true; }

bool Lora::begin(float freqMhz, float bwKhz, uint8_t sf, int8_t txPowerDbm) {
    s_loraSpi.begin(LORA_SPI_SCK, LORA_SPI_MISO, LORA_SPI_MOSI, LORA_PIN_NSS);
    freqMhz_ = freqMhz;
    // begin(freq, bw, sf, cr, syncWord, power, preambleLen, tcxoVoltage)
    int st = s_radio.begin(freqMhz, bwKhz, sf, 5,
                           RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                           txPowerDbm, 8, LORA_TCXO_V);
    ready_ = (st == RADIOLIB_ERR_NONE);
    if (ready_) s_radio.setDio1Action(onDio1);
    return ready_;
}

bool Lora::setFrequency(float freqMhz) {
    if (!ready_) return false;
    freqMhz_ = freqMhz;
    return s_radio.setFrequency(freqMhz) == RADIOLIB_ERR_NONE;
}

bool Lora::transmit(const uint8_t* data, size_t len) {
    if (!ready_) return false;
    int st = s_radio.transmit(const_cast<uint8_t*>(data), len);
    s_radio.startReceive();   // return to RX after a blocking TX
    s_rxFlag = false;
    return st == RADIOLIB_ERR_NONE;
}

void Lora::startReceive() {
    if (!ready_) return;
    s_rxFlag = false;
    s_radio.startReceive();
}

bool Lora::receive(RxPacket& out) {
    if (!ready_ || !s_rxFlag) return false;
    s_rxFlag = false;
    int len = s_radio.getPacketLength();
    if (len <= 0 || len > (int)sizeof(out.data)) { s_radio.startReceive(); return false; }
    int st = s_radio.readData(out.data, len);
    if (st == RADIOLIB_ERR_NONE) {
        out.len = len;
        out.rssi = s_radio.getRSSI();
        out.snr  = s_radio.getSNR();
        out.freqErrHz = 0;   // SX126x doesn't expose per-packet freq error
        s_radio.startReceive();
        return true;
    }
    s_radio.startReceive();
    return false;
}

float Lora::channelRssi() {
    if (!ready_) return -200.f;
    return s_radio.getRSSI(/*skipReceive=*/false);
}
