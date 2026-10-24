# Huginn-RF

Modular wireless-security firmware for the **M5Stack Cardputer ADV** (ESP32-S3)
with the **LoRa-1262 cap** (Semtech SX1262 + ATGM336H GPS).

Huginn-RF is a research/pentest multitool in the spirit of Bruce and
ESP32-Marauder, built around two ideas:

1. **Everything is a module.** Each capability — WiFi, BLE, Sub-GHz, IR, RFID,
   LoRa — is a self-contained `Module` that registers itself with a central
   registry and shows up in the menu. Adding a feature means adding a module,
   not touching the core.
2. **Config lives on the SD card, not in the binary.** Targets, payload lists,
   channel plans and module options are loaded from YAML on the SD card at
   runtime (the "voidrecon / NovaLure" pattern), so you reconfigure in the
   field without reflashing.

> ⚠️ **Authorized use only.** This firmware transmits on licensed/shared radio
> bands and can disrupt wireless devices. Use it only on hardware and networks
> you own or have **explicit written permission** to test. You are responsible
> for complying with your local radio, wiretap, and computer-misuse laws.
> See [`docs/LEGAL.md`](docs/LEGAL.md). Offensive modules refuse to run until
> you acknowledge this on-device.

## Status

Early scaffold. What's implemented vs. stubbed:

| Area | Module | State |
|------|--------|-------|
| Core framework (registry, menu, SD, YAML config, logger) | — | ✅ implemented |
| HAL: SX1262 LoRa radio | `hal/Lora` | ✅ implemented |
| HAL: ATGM336H GPS | `hal/Gps` | ✅ implemented |
| **Wardriving** (WiFi APs + BLE + LoRa) → WiGLE CSV with GPS | `modules/wardrive` | ✅ implemented |
| **Who's Around** — live nearby-device presence + lock-on RSSI locate | `modules/recon` | ✅ implemented |
| **Encrypted P2P messenger** (AES-GCM over 915 MHz) | `modules/messenger` | ✅ implemented |
| **IR transmitter** — raw `.ir`/TV-B-Gone + typed NEC/Samsung/Sony codes | `modules/ir` | ✅ implemented |
| **Scripts** — run `.hsc` macro scripts from SD | `modules/script` | ✅ implemented |
| WiFi: scan, sniffer→pcap, EAPOL/PMKID capture, **targeted** deauth, Evil Portal, capture verify, **probe-SSID harvest** | `modules/wifi` | ✅ implemented |
| LoRa raw sniffer → SD log | `modules/lora_sniffer` | ✅ implemented |
| BLE scan | `modules/ble` | ✅ scan; HID "BadBLE" in the `cardputer_adv_badble` image |
| BadUSB (HID injection) | `modules/badusb` | ✅ implemented (DuckyScript + USB-HID) |

Every module in this table builds in the single `cardputer_adv` profile and
runs on the Cardputer ADV + LoRa-1262 cap with no add-on hardware.

## Roadmap — toward Bruce parity and beyond

Target feature set (✅ done, 🟡 in progress, ⬜ planned). The goal is everything
Bruce/Marauder do on comparable hardware, plus the LoRa/GPS capabilities this
cap unlocks.

Works with the hardware on hand (Cardputer ADV + LoRa-1262 cap):

- **Recon / situational awareness:** ✅ wardriving (WiFi+BLE+LoRa → WiGLE,
  GPS-stamped), ✅ live "who's around" presence, ✅ lock-on RSSI proximity
  locate, ✅ probe-request SSID harvest, ⬜ presence logging over time.
- **WiFi:** ✅ scan, ✅ sniffer→pcap, ✅ EAPOL/PMKID capture, ✅ targeted
  deauth (bounded, reauth-capture), ✅ capture verify + hashcat recipe,
  ✅ evil portal, ⬜ on-device `22000` conversion.
- **BLE:** ✅ scan, ✅ HID keyboard ("BadBLE", separate `cardputer_adv_badble`
  image — the HID stack doesn't share an image with the scanner), ⬜ GATT
  enumerator.
- **LoRa (SX1262, this cap):** ✅ AES messenger, ✅ raw LoRa sniffer,
  ⬜ Meshtastic observer, ⬜ RF spectrum sweep. (LoRaWAN sniff/fuzz dropped —
  one SX1262 can't follow LoRaWAN's channel+SF hopping, so it caught almost
  nothing; not worth shipping on this cap.)
- **IR:** ✅ TX raw/.ir/TV-B-Gone, ✅ NEC/NEC-ext/Samsung/Sony encoders
  ("type a code"), ⬜ RC5/RC6 (Manchester).
- **USB/HID:** ✅ BadUSB DuckyScript, ⬜ payload library, ⬜ USB mass-storage.
- **Platform:** ✅ scripting engine (.hsc macros from SD), ⬜ more script verbs,
  ⬜ on-device config editor, ⬜ OTA update.

Needs add-on hardware you don't currently have (left out until then):

- **Sub-GHz OOK/ASK cloning** — a CC1101 module (the SX1262 cap does chirp
  LoRa, not general OOK remote capture).
- **RFID/NFC** — a PN532 or RC522 reader.
- **IR receive/copy** — an external IR receiver on a GPIO pin.
- **NRF24 / 2.4 GHz toys** — an nRF24L01 module.

The code for the CC1101 and PN532 modules lives in git history and can return
behind its own build the day that hardware is attached.

### What this project will *not* ship as turnkey

Pure denial-of-service and indiscriminate mass-targeting have no authorized-test
use that a *targeted* tool can't serve, so they aren't shipped as one-button
features: RF/WiFi/BLE **jammers**, **"deauth everything"** floods, and
**mass BLE pairing-popup spam**. Where an engagement genuinely needs a
disruptive step (e.g. forcing a single client to reauth for handshake capture),
implement it **targeted, bounded, and logged** against gear you're scoped to
test. See `docs/LEGAL.md`.

## Build & flash

```bash
# PlatformIO — scoped to the Cardputer ADV + LoRa-1262 cap
pio run -e cardputer_adv            # main build (everything except BadBLE)
pio run -e cardputer_adv -t upload  # flash over USB-C
pio device monitor                  # serial console

# Optional separate image for BLE HID ("BadBLE") keystroke injection:
pio run -e cardputer_adv_badble -t upload
```

See [`docs/HARDWARE.md`](docs/HARDWARE.md) for pin maps and the CC1101 variant,
and [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for how modules/config work.

## SD card layout

Copy the `sd_card/` tree to the root of a FAT32 SD card:

```
/huginn/
  config.yaml            # global + per-module config
  payloads/
    badusb/*.txt         # DuckyScript payloads
    lorawan/*.yaml       # join-fuzzer mutation profiles (test server only)
  ir/*.ir                # saved IR codes
  captures/              # runtime output (wardrive CSV, logs) — gitignored
```

## License

See [`LICENSE`](LICENSE). Research/educational use; no warranty.
