// Logger.h — timestamped logging to serial and (optionally) the SD card.
// Every transmit action should be logged so an engagement has an audit trail.
#pragma once
#include <Arduino.h>

class SdCard;
class Gps;

class Logger {
public:
    void begin(SdCard* sd, Gps* gps, bool toSd);

    void info(const char* fmt, ...);
    void warn(const char* fmt, ...);
    // Use for every RF/HID transmit action (creates the audit trail).
    void tx(const char* module, const char* fmt, ...);

private:
    void write(const char* level, const char* tag, const char* msg);

    SdCard* sd_   = nullptr;
    Gps*    gps_  = nullptr;
    bool    toSd_ = false;
};
