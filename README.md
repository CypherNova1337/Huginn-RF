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
| Sub-GHz / LoRa **wardriving → WiGLE CSV** | `modules/wardrive` | ✅ implemented |
| **Encrypted P2P messenger** (AES-GCM over 915 MHz) | `modules/messenger` | ✅ implemented |
| **IR transmitter** (TV-B-Gone / universal remote) | `modules/ir` | ✅ implemented |
| LoRaWAN **OTAA join fuzzer** (your own test server) | `modules/lorawan_fuzzer` | 🟡 harness + guards, payload mutators are yours |
| WiFi: AP scan, sniffer→pcap, EAPOL/PMKID capture, Evil Portal | `modules/wifi` | ✅ implemented (deauth left as gated hook) |
| BLE (scan / spam / HID) | `modules/ble` | 🟡 interface + scan; spam/HID hooks stubbed |
| Sub-GHz raw OOK/ASK (CC1101) | `modules/subghz` | 🟡 interface stubbed (needs CC1101, see HARDWARE.md) |
| RFID/NFC | `modules/rfid` | 🟡 interface stubbed (needs reader module) |
| BadUSB (HID injection) | `modules/badusb` | 🟡 interface + DuckyScript parser stub |

"🟡 stubbed" modules compile and appear in the menu, but their offensive
actions are left as clearly-marked `TODO` hooks for you to fill in against your
own test gear.

## Roadmap — toward Bruce parity and beyond

Target feature set (✅ done, 🟡 in progress, ⬜ planned). The goal is everything
Bruce/Marauder do on comparable hardware, plus the LoRa/GPS capabilities this
cap unlocks.

- **WiFi:** ✅ scan, ✅ sniffer→pcap, ✅ EAPOL/PMKID capture, ✅ evil portal,
  ⬜ beacon spam, ⬜ probe/station mapper, ⬜ targeted deauth (authorized
  reauth-capture), ⬜ handshake→hashcat `22000` export, ⬜ Wireguard/pivot.
- **BLE:** ✅ scan, ⬜ HID keyboard ("BadBLE"), ⬜ advertisement spam
  (rate-limited), ⬜ GATT enumerator.
- **Sub-GHz (CC1101):** 🟡 scaffold, ⬜ OOK/ASK capture, ⬜ replay, ⬜ protocol
  decode (OOK remotes), ⬜ frequency/RSSI spectrum view.
- **LoRa (SX1262, this cap):** ✅ wardrive→WiGLE, ✅ AES messenger,
  ✅ OTAA join fuzzer, ⬜ raw LoRa sniffer, ⬜ LoRaWAN uplink sniffer,
  ⬜ Meshtastic observer, ⬜ RF spectrum sweep.
- **IR:** ✅ TX raw/.ir/TV-B-Gone, ⬜ NEC/RC5/SIRC/Samsung encoders,
  ⬜ RX decode (needs external receiver on GPIO).
- **RFID/NFC:** 🟡 scaffold, ⬜ 13.56 MHz read/clone/write (PN532/RC522),
  ⬜ 125 kHz, ⬜ emulation/chameleon.
- **USB/HID:** ✅ BadUSB DuckyScript, ⬜ payload library, ⬜ USB mass-storage.
- **Platform:** ⬜ scripting engine (run modules from an SD script),
  ⬜ on-device config editor, ⬜ OTA update, ⬜ NRF24 scanner.

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
# PlatformIO
pio run -e cardputer_adv            # build
pio run -e cardputer_adv -t upload  # flash over USB-C
pio device monitor                  # serial console
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
