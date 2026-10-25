#include "Input.h"
#include "Display.h"
#include <M5Cardputer.h>

// Cardputer has no dedicated arrow cluster; following the common Bruce-style
// convention we use the semicolon/period/comma/slash keys as a nav pad.
//   ';' up   '.' down   ',' left   '/' right
// Enter = enter key, Back = the top-left "Esc"/backtick key ('`').
void Input::begin() {
    M5Cardputer.update();
}

KeyEvent Input::poll() {
    M5Cardputer.update();
    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed())
        return {};

    auto st = M5Cardputer.Keyboard.keysState();
    if (st.enter) return {Key::Enter, 0};

    for (char c : st.word) {
        switch (c) {
            case ';': return {Key::Up, 0};
            case '.': return {Key::Down, 0};
            case ',': return {Key::Left, 0};
            case '/': return {Key::Right, 0};
            case '`': return {Key::Back, 0};   // Esc
            default:  return {Key::Char, c};
        }
    }
    if (st.del) return {Key::Back, 0};
    return {};
}

bool Input::readLine(const char* prompt, String& out, bool mask, bool keepInitial) {
    auto& d = M5Cardputer.Display;
    if (!keepInitial) out = "";
    uint32_t lastDraw = 0;
    for (;;) {
        M5Cardputer.update();

        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            auto st = M5Cardputer.Keyboard.keysState();
            if (st.enter) return true;
            if (st.del && out.length()) out.remove(out.length() - 1);
            for (char c : st.word) {
                if (c == '`') return false;          // Esc cancels
                if (c >= 32 && c < 127) out += c;
            }
            lastDraw = 0; // force redraw
        }

        if (millis() - lastDraw > 80) {
            lastDraw = millis();
            int y = d.height() / 2 - 10;
            d.fillRect(0, y - 2, d.width(), 24, 0x0000);
            d.setTextColor(0x8410, 0x0000);
            d.setCursor(4, y - 12); d.print(prompt);
            d.setTextColor(0xFFE0, 0x0000);
            d.setCursor(4, y);
            if (mask) { for (size_t i = 0; i < out.length(); ++i) d.print('*'); }
            else d.print(out);
            d.print('_');
        }
        delay(5);
    }
}
