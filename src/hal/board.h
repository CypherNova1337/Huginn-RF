// board.h — hardware pin map for the Cardputer ADV + LoRa-1262 cap.
//
// Verified against the LoRa-1262 cap silkscreen:
//   SX1262  NSS=G5 RST=G3 IRQ/DIO1=G4 BUSY=G6  SCK=G40 MOSI=G14 MISO=G39
//   ATGM336H GPS  RX=G13  TX=G15
// The SX1262 shares the SPI bus (SCK/MOSI/MISO) with the microSD card; only the
// chip-selects differ (SD=G12, LoRa=G5). So SD and LoRa use ONE shared bus.
#pragma once

#if !defined(BOARD_CARDPUTER_ADV)
#warning "Huginn-RF is tuned for the Cardputer ADV; pin maps may be wrong on other boards."
#endif

// ---------------------------------------------------------------------------
// Shared SPI bus (microSD + SX1262 LoRa cap). See hal/Bus.*.
// ---------------------------------------------------------------------------
#define SPI_PIN_SCK   40
#define SPI_PIN_MISO  39
#define SPI_PIN_MOSI  14

#define SD_PIN_CS     12   // microSD chip-select

// ---------------------------------------------------------------------------
// SX1262 (LoRa / sub-GHz) on the shared bus.
// ---------------------------------------------------------------------------
#define LORA_PIN_NSS   5   // SPI chip-select
#define LORA_PIN_DIO1  4   // IRQ
#define LORA_PIN_RST   3   // reset
#define LORA_PIN_BUSY  6   // busy
#define LORA_TCXO_V    1.8 // most of these modules use a 1.8V TCXO; set 0 if not

// ---------------------------------------------------------------------------
// ATGM336H GPS (UART). If you get satellites but never a fix, try swapping
// RX/TX — the cap silk labels them from the module's point of view.
// ---------------------------------------------------------------------------
#define GPS_UART_NUM   1
// Swapped per hardware test (rx=0 with 13/15): the cap's "G13 RX / G15 TX" are
// labeled from the GPS module's side, so the ESP reads on G15, writes on G13.
#define GPS_PIN_RX    15   // ESP32 RX  <- GPS TX (G15)
#define GPS_PIN_TX    13   // ESP32 TX  -> GPS RX (G13)
#define GPS_BAUD    9600

// ---------------------------------------------------------------------------
// IR emitter (built-in, TX only). Verify on the ADV if IR doesn't work.
// ---------------------------------------------------------------------------
#define IR_TX_PIN     44

// Add-on hardware not present on this setup (CC1101 sub-GHz, PN532 RFID) had
// their pin maps here; they were removed with their modules. Restore them from
// git history if that hardware is ever attached.
