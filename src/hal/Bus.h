// Bus.h — the one SPI bus shared by the microSD card and the SX1262 LoRa cap.
// They sit on the same SCK/MOSI/MISO and differ only in chip-select, so both
// must drive a single SPIClass. Call spiBegin() once (SdCard does it first),
// then hand spiBus() to SD.begin() and to the RadioLib Module.
#pragma once
#include <SPI.h>

namespace hal {
    SPIClass& spiBus();   // the shared instance (not begun by this call)
    void      spiBegin();  // idempotent: begins the bus on the shared pins once
}
