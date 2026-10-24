// Module.h — the interface every Huginn-RF feature implements, plus the shared
// Context that gives a module access to device services.
#pragma once
#include <Arduino.h>

class Display;
class Input;
class SdCard;
class Config;
class Logger;
class Lora;
class Gps;

// Top-level menu grouping.
enum class Category : uint8_t {
    WiFi,
    Bluetooth,
    SubGHz,
    LoRa,
    IR,
    RFID,
    USB,
    Tools,
    _count
};

const char* categoryName(Category c);

// Shared services handed to every module. Owned by App; modules borrow.
struct Context {
    Display* display = nullptr;
    Input*   input   = nullptr;
    SdCard*  sd      = nullptr;
    Config*  config  = nullptr;
    Logger*  log     = nullptr;
    Lora*    lora    = nullptr;
    Gps*     gps     = nullptr;

    // Set once the user has acknowledged authorized-use (config or on-device).
    bool authorized = false;
};

// A module runs its own screen loop. The menu calls enter() once, then loop()
// repeatedly until loop() returns false (meaning "back to menu"), then exit().
class Module {
public:
    virtual ~Module() = default;

    virtual const char* name() const = 0;
    virtual Category    category() const = 0;

    // Short one-line description shown in the menu footer.
    virtual const char* description() const { return ""; }

    // True if this module transmits / performs offensive actions and therefore
    // requires the authorized-use acknowledgement before running.
    virtual bool requiresAuthorization() const { return false; }

    virtual void enter(Context& ctx) {}
    // Return false to exit the module and return to the menu.
    virtual bool loop(Context& ctx) { return false; }
    virtual void exit(Context& ctx) {}
};
