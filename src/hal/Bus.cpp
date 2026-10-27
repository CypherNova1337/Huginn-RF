#include "Bus.h"
#include "board.h"
#include <Arduino.h>

// Constructed at static-init (safe — no bus access), begun later in setup().
static SPIClass s_spi(HSPI);
static bool     s_begun = false;

SPIClass& hal::spiBus() { return s_spi; }

void hal::spiBegin() {
    if (s_begun) return;
    // Park every chip-select on the shared bus HIGH (deselected) BEFORE the bus
    // comes up. This is critical: the SX1262 powers up with its NSS floating,
    // and a floating/low NSS lets the radio drive the shared MISO line — which
    // corrupts SD.begin() and makes the card "disappear" whenever the LoRa cap
    // is plugged in. Driving NSS high first hands the bus cleanly to the SD card
    // at mount time; RadioLib re-takes NSS when the radio is later initialised.
    pinMode(LORA_PIN_NSS, OUTPUT); digitalWrite(LORA_PIN_NSS, HIGH);
    pinMode(SD_PIN_CS,    OUTPUT); digitalWrite(SD_PIN_CS,    HIGH);
    // Hold the radio in reset until Lora::begin() brings it up, so it can't
    // chatter on the bus during SD mount.
    pinMode(LORA_PIN_RST, OUTPUT); digitalWrite(LORA_PIN_RST, LOW);

    s_spi.begin(SPI_PIN_SCK, SPI_PIN_MISO, SPI_PIN_MOSI);
    s_begun = true;
}
