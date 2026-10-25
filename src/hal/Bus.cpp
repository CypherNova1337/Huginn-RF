#include "Bus.h"
#include "board.h"

// Constructed at static-init (safe — no bus access), begun later in setup().
static SPIClass s_spi(HSPI);
static bool     s_begun = false;

SPIClass& hal::spiBus() { return s_spi; }

void hal::spiBegin() {
    if (s_begun) return;
    s_spi.begin(SPI_PIN_SCK, SPI_PIN_MISO, SPI_PIN_MOSI);
    s_begun = true;
}
