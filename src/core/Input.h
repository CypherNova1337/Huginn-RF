// Input.h — polls the Cardputer's 56-key keyboard and normalizes the keys the
// UI cares about into a small event enum, plus raw text entry.
#pragma once
#include <Arduino.h>

enum class Key : uint8_t {
    None, Up, Down, Left, Right, Enter, Back, Char
};

struct KeyEvent {
    Key  key = Key::None;
    char ch  = 0;      // valid when key == Key::Char
};

class Input {
public:
    void begin();

    // Non-blocking: returns the next key event, or {None} if nothing.
    KeyEvent poll();

    // Blocking line editor drawn at the given display row. Returns when Enter
    // is pressed (true) or Esc/Back (false). `out` holds the typed text.
    bool readLine(const char* prompt, String& out, bool mask = false);

private:
    uint32_t lastPollMs_ = 0;
};
