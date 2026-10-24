#include "Display.h"
#include <M5Cardputer.h>
#include <stdarg.h>

static constexpr uint16_t COL_BG     = 0x0000; // black
static constexpr uint16_t COL_HEADER = 0x5D1F; // blue-ish
static constexpr uint16_t COL_FOOT   = 0x8410; // grey
static constexpr uint16_t COL_SEL_BG = 0x2945;
static constexpr uint16_t COL_SEL_FG = 0xFFE0; // yellow

void Display::begin(uint8_t brightness) {
    auto& d = M5Cardputer.Display;
    d.setRotation(1);
    d.setTextSize(1);
    d.setFont(&fonts::Font0);
    setBrightness(brightness);
    lineH_   = d.fontHeight() + 2;
    bodyTop_ = lineH_ + 4;
    bodyRows_ = (d.height() - bodyTop_ - lineH_ - 4) / lineH_;
    clear();
}

void Display::setBrightness(uint8_t b) { M5Cardputer.Display.setBrightness(b); }

void Display::clear() { M5Cardputer.Display.fillScreen(COL_BG); }

void Display::header(const char* title) {
    auto& d = M5Cardputer.Display;
    d.fillRect(0, 0, d.width(), lineH_ + 2, COL_HEADER);
    d.setTextColor(0xFFFF, COL_HEADER);
    d.setCursor(4, 2);
    d.print(title);
}

void Display::footer(const char* hint) {
    auto& d = M5Cardputer.Display;
    int y = d.height() - lineH_ - 2;
    d.fillRect(0, y, d.width(), lineH_ + 2, COL_BG);
    d.setTextColor(COL_FOOT, COL_BG);
    d.setCursor(4, y + 1);
    d.print(hint);
}

void Display::line(int row, const char* text, uint16_t color) {
    auto& d = M5Cardputer.Display;
    int y = bodyTop_ + row * lineH_;
    d.fillRect(0, y, d.width(), lineH_, COL_BG);
    d.setTextColor(color, COL_BG);
    d.setCursor(4, y);
    d.print(text);
}

void Display::printf(int row, uint16_t color, const char* fmt, ...) {
    char buf[128];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    line(row, buf, color);
}

void Display::list(const char* const* items, int count, int selected,
                   const char* title, const char* hint) {
    auto& d = M5Cardputer.Display;
    clear();
    header(title);

    // Scroll window so the selection stays visible.
    int first = 0;
    if (selected >= bodyRows_) first = selected - bodyRows_ + 1;

    for (int i = 0; i < bodyRows_ && (first + i) < count; ++i) {
        int idx = first + i;
        int y = bodyTop_ + i * lineH_;
        bool sel = (idx == selected);
        if (sel) d.fillRect(0, y, d.width(), lineH_, COL_SEL_BG);
        d.setTextColor(sel ? COL_SEL_FG : 0xFFFF, sel ? COL_SEL_BG : COL_BG);
        d.setCursor(6, y);
        d.print(sel ? "> " : "  ");
        d.print(items[idx]);
    }
    footer(hint);
}
