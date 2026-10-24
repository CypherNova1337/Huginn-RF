// Display.h — thin helper over the Cardputer's M5GFX canvas. Modules draw
// through this so the look stays consistent (header bar, body, footer hint).
#pragma once
#include <Arduino.h>

class Display {
public:
    void begin(uint8_t brightness = 160);
    void setBrightness(uint8_t b);

    void clear();
    // Standard chrome: title bar at top, hint line at bottom.
    void header(const char* title);
    void footer(const char* hint);

    // Body text helpers. Row is a line index inside the body area.
    void line(int row, const char* text, uint16_t color = 0xFFFF);
    void printf(int row, uint16_t color, const char* fmt, ...);

    // A simple vertical list with a highlighted selection (used by the menu).
    void list(const char* const* items, int count, int selected,
              const char* title, const char* hint);

    int bodyRows() const { return bodyRows_; }

private:
    int bodyRows_ = 0;
    int bodyTop_  = 0;
    int lineH_    = 0;
};
