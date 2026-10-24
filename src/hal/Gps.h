// Gps.h — ATGM336H NMEA reader (TinyGPSPlus) with a cached fix.
#pragma once
#include <Arduino.h>

class Gps {
public:
    struct Fix {
        double   lat = 0, lon = 0;
        double   altMeters = 0;
        double   speedKmph = 0;
        uint8_t  satellites = 0;
        uint16_t year = 0; uint8_t month = 0, day = 0;
        uint8_t  hour = 0, minute = 0, second = 0;
        bool     valid = false;
    };

    void begin(uint32_t baud);
    // Pump NMEA from the UART; call often from the main/module loop.
    void update();

    bool       hasFix() const { return fix_.valid; }
    const Fix& fix() const { return fix_; }

    // ISO-8601 UTC of the last fix, for WiGLE CSV ("YYYY-MM-DD HH:MM:SS").
    String isoTime() const;

private:
    Fix fix_;
};
