# Flashing Huginn-RF

Start-to-finish: get the firmware onto your computer and flash it to the
Cardputer ADV over USB-C.

## 0. What you need

- Cardputer ADV + a USB-C **data** cable (not charge-only).
- A microSD card (FAT32).
- A computer with Python 3 and `git`.

The ADV's ESP32-S3 uses **native USB**, so macOS/Linux need **no driver**. On
Windows it usually enumerates as a COM port on its own; only if it doesn't,
install a generic USB-serial / "ESP32-S3 USB CDC" driver.

## 1. Install the PlatformIO CLI

```bash
python3 -m pip install --upgrade platformio
pio --version
```

If `pio` isn't on your PATH, it lives at `~/.platformio/penv/bin/pio` — add that
to PATH, or use `python3 -m platformio` wherever `pio` appears below.

## 2. Get the firmware

```bash
git clone https://github.com/CypherNova1337/Huginn-RF.git
cd Huginn-RF
# already cloned? -> git checkout main && git pull
```

## 3. Connect the device and find its port

- **macOS:** `ls /dev/cu.usbmodem*`
- **Linux:** `ls /dev/ttyACM*`  (permission denied later? `sudo usermod -aG dialout $USER`, then log out/in)
- **Windows:** Device Manager -> Ports (COM & LPT) -> note the new `COMx`

## 4. Build and flash

```bash
pio run -e cardputer_adv -t upload
```

Compiles and writes over USB; the port is auto-detected. The first build
downloads the toolchain and libraries (a few minutes); later builds are fast.

**If the upload won't start** (`No serial data received` / `Failed to
connect`), enter the ESP32-S3 bootloader manually:

1. Hold the **G0 / BtnA** button.
2. Tap **RESET** while still holding G0.
3. Release G0.
4. Re-run `pio run -e cardputer_adv -t upload`.
5. Press **RESET** once when it finishes, to boot the firmware.

Force a specific port if auto-detect picks the wrong one:

```bash
pio run -e cardputer_adv -t upload --upload-port /dev/cu.usbmodemXXXX   # or COM5, /dev/ttyACM0
```

## 5. Prepare the microSD card

The firmware reads its config and payloads from the card. Format it **FAT32**,
then copy the repo's `sd_card/huginn/` folder to the card root so the card has
`/huginn/...`:

```bash
cp -R sd_card/huginn /Volumes/HUGINN/        # macOS (use your card's volume name)
cp -R sd_card/huginn /media/$USER/HUGINN/    # Linux
```

On Windows, drag the `sd_card\huginn` folder onto the card's drive. Then edit
`/huginn/config.yaml` on the card (region, `authorized_ack`, PSK, etc.) and
insert the card before powering on.

## 6. Serial console (optional)

```bash
pio device monitor -b 115200
```

Shows boot logs (SD mounted? LoRa found? GPS fix). Ctrl-C to exit, and close the
monitor before the next upload (it holds the port).

## 7. BadBLE image (only when you want HID injection)

BLE HID ships as its own image. Flash it only for that purpose:

```bash
pio run -e cardputer_adv_badble -t upload
```

Flash `cardputer_adv` again to return to the normal firmware.

## First-flash caveats

1. **Verify the pin maps in `src/hal/board.h`** against your LoRa-1262 cap
   revision. CI proves the firmware *compiles*, not that the SX1262/GPS pins
   match your board — wrong pins show up as "radio not found."
2. If the screen or keyboard misbehaves on the ADV, the panel/keyboard
   overrides also live in `board.h`.
