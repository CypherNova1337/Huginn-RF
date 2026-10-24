# Hardware notes — Cardputer ADV + LoRa-1262 cap

## Cardputer ADV

- **MCU:** ESP32-S3 (dual-core Xtensa LX7, native USB-OTG, 2.4 GHz WiFi + BT5/BLE).
- **Display:** 1.14" TFT (ST7789-class), driven via `M5Cardputer`/`M5GFX`.
- **Keyboard:** 56-key matrix, read through the `M5Cardputer.Keyboard` API.
- **IR:** built-in IR **emitter** (TX only — there is no stock IR receiver; a
  receiver needs an external module on the Grove/GPIO header).
- **USB-C:** used for flashing, serial console, and USB-HID (BadUSB).
- **SD:** microSD slot (SPI). FAT32.

> The ADV is newer than the original Cardputer. If `M5Cardputer.begin()` mis-detects
> the panel or keyboard, pin overrides live in `src/hal/board.h` under
> `BOARD_CARDPUTER_ADV`. Verify against your unit's schematic and adjust.

## LoRa-1262 cap (SX1262 + ATGM336H GPS)

The cap sits on the Cardputer's expansion header. Two devices:

### SX1262 (LoRa / sub-GHz chirp radio)
Connected over SPI. Fill in the exact pins for your cap revision in
`src/hal/board.h`:

```c
// src/hal/board.h  (verify against your cap's silk/schematic!)
#define LORA_SPI_SCK    36
#define LORA_SPI_MISO   37
#define LORA_SPI_MOSI   35
#define LORA_PIN_NSS     7   // SPI chip-select
#define LORA_PIN_DIO1    9   // IRQ
#define LORA_PIN_RST    13   // reset
#define LORA_PIN_BUSY   11   // busy
```

- **Default region:** 915 MHz (US915). Change `radio.region` in `config.yaml`.
- The SX1262 does **chirp (LoRa) + (G)FSK/OOK**. It can do raw OOK, but it is
  *not* a drop-in for a CC1101 for arbitrary sub-GHz remote cloning — timing
  resolution and filtering differ. For general OOK/ASK capture/replay (garage
  remotes etc.) use the CC1101 variant below.

### ATGM336H (GPS)
A UART NMEA module. Wire to a spare UART:

```c
#define GPS_UART_NUM     1
#define GPS_PIN_RX      17   // ESP32 RX  <- GPS TX
#define GPS_PIN_TX      18   // ESP32 TX  -> GPS RX
#define GPS_BAUD      9600
```

Parsed with TinyGPSPlus; used by the wardrive module for position stamping.

## Optional: CC1101 for raw sub-GHz (OOK/ASK)

For copying/replaying simple OOK/ASK remotes you want a CC1101. You can't run
it and the LoRa cap at once on the same SPI-CS without careful bus sharing, so
the simplest path is to swap the cap for a CC1101 breakout wired to the GPIO
header, and build the `cardputer_adv_cc1101` env:

```c
#define CC1101_PIN_SCK   36
#define CC1101_PIN_MISO  37
#define CC1101_PIN_MOSI  35
#define CC1101_PIN_CS     7
#define CC1101_PIN_GDO0   9
#define CC1101_PIN_GDO2  10
```

RadioLib provides the CC1101 driver; the `subghz` module targets it.

## Optional: RFID/NFC reader

The ADV has no onboard RFID. Add a PN532 (I²C/SPI) or MFRC522 (SPI) on the
Grove/GPIO header; the `rfid` module is written against a reader abstraction so
either can be dropped in.
