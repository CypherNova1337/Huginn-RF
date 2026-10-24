// board.h — all hardware pin maps for the Cardputer ADV + LoRa-1262 cap.
//
// VERIFY these against your specific cap revision's schematic/silkscreen before
// transmitting. Wrong SX1262 pins are the #1 cause of "radio not found".
#pragma once

#if !defined(BOARD_CARDPUTER_ADV)
#warning "Huginn-RF is tuned for the Cardputer ADV; pin maps may be wrong on other boards."
#endif

// ---------------------------------------------------------------------------
// microSD (SPI) — on the Cardputer the SD shares the main SPI bus.
// ---------------------------------------------------------------------------
#define SD_PIN_SCK      40
#define SD_PIN_MISO     39
#define SD_PIN_MOSI     14
#define SD_PIN_CS       12

// ---------------------------------------------------------------------------
// LoRa-1262 cap — SX1262 over its own SPI pins (verify!).
// ---------------------------------------------------------------------------
#define LORA_SPI_SCK    36
#define LORA_SPI_MISO   37
#define LORA_SPI_MOSI   35
#define LORA_PIN_NSS     7   // SPI chip-select
#define LORA_PIN_DIO1    9   // IRQ line
#define LORA_PIN_RST    13   // reset
#define LORA_PIN_BUSY   11   // busy line
#define LORA_TCXO_V      1.8 // many SX1262 modules use a 1.8V TCXO; 0 = none

// ---------------------------------------------------------------------------
// ATGM336H GPS (UART).
// ---------------------------------------------------------------------------
#define GPS_UART_NUM     1
#define GPS_PIN_RX      17   // ESP32 RX  <- GPS TX
#define GPS_PIN_TX      18   // ESP32 TX  -> GPS RX
#define GPS_BAUD      9600

// ---------------------------------------------------------------------------
// IR emitter (built-in). TX only on stock hardware.
// ---------------------------------------------------------------------------
#define IR_TX_PIN       44

// Add-on hardware not present on this setup (CC1101 sub-GHz, PN532 RFID) had
// their pin maps here; they were removed with their modules. Restore them from
// git history (or docs/HARDWARE.md) if that hardware is ever attached.
