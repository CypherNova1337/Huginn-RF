#include "Display.h"
#include <M5Cardputer.h>
#include <stdarg.h>

static constexpr uint16_t COL_BG     = 0x0000; // black
static constexpr uint16_t COL_HEADER = 0x5D1F; // blue-ish
static constexpr uint16_t COL_FOOT   = 0x8410; // grey
static constexpr uint16_t COL_SEL_BG = 0x2945;
static constexpr uint16_t COL_SEL_FG = 0xFFE0; // yellow

// All drawing goes to this off-screen canvas, then present() blits it to the
// panel in one shot. Without this, clearing + redrawing the live panel every
// frame strobes the whole screen.
static M5Canvas s_cv(&M5Cardputer.Display);

void Display::begin(uint8_t brightness) {
    auto& d = M5Cardputer.Display;
    d.setRotation(1);
    setBrightness(brightness);

    // Try to allocate a full-screen 16-bit sprite; fall back to direct draw.
    s_cv.setColorDepth(16);
    canvas_ = s_cv.createSprite(d.width(), d.height());

    // Configure fonts on whichever surface we draw to.
    if (canvas_) { s_cv.setFont(&fonts::Font0); s_cv.setTextSize(1); }
    d.setFont(&fonts::Font0);
    d.setTextSize(1);

    lineH_   = d.fontHeight() + 2;
    bodyTop_ = lineH_ + 4;
    bodyRows_ = (d.height() - bodyTop_ - lineH_ - 4) / lineH_;
    clear();
    present();
}

void Display::setBrightness(uint8_t b) { M5Cardputer.Display.setBrightness(b); }

// Draw target: the canvas when available, otherwise the live panel. Both derive
// from lgfx::LovyanGFX, so we draw through that common base.
#define GFX (canvas_ ? (lgfx::LovyanGFX&)s_cv : (lgfx::LovyanGFX&)M5Cardputer.Display)

void Display::clear() { GFX.fillScreen(COL_BG); }

void Display::present() {
    if (canvas_) s_cv.pushSprite(0, 0);
}

void Display::header(const char* title) {
    auto& g = GFX;
    g.fillRect(0, 0, g.width(), lineH_ + 2, COL_HEADER);
    g.setTextColor(0xFFFF, COL_HEADER);
    g.setCursor(4, 2);
    g.print(title);
}

void Display::footer(const char* hint) {
    auto& g = GFX;
    int y = g.height() - lineH_ - 2;
    g.fillRect(0, y, g.width(), lineH_ + 2, COL_BG);
    g.setTextColor(COL_FOOT, COL_BG);
    g.setCursor(4, y + 1);
    g.print(hint);
}

void Display::line(int row, const char* text, uint16_t color) {
    auto& g = GFX;
    int y = bodyTop_ + row * lineH_;
    g.fillRect(0, y, g.width(), lineH_, COL_BG);
    g.setTextColor(color, COL_BG);
    g.setCursor(4, y);
    g.print(text);
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
    auto& g = GFX;
    clear();
    header(title);

    int first = 0;
    if (selected >= bodyRows_) first = selected - bodyRows_ + 1;

    for (int i = 0; i < bodyRows_ && (first + i) < count; ++i) {
        int idx = first + i;
        int y = bodyTop_ + i * lineH_;
        bool sel = (idx == selected);
        if (sel) g.fillRect(0, y, g.width(), lineH_, COL_SEL_BG);
        g.setTextColor(sel ? COL_SEL_FG : 0xFFFF, sel ? COL_SEL_BG : COL_BG);
        g.setCursor(6, y);
        g.print(sel ? "> " : "  ");
        g.print(items[idx]);
    }
    footer(hint);
}
