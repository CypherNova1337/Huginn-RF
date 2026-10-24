// Messenger.h — point-to-point encrypted text chat over LoRa (AES-256-GCM with
// a pre-shared key). Each message frame is: [nonce:12][ciphertext][tag:16].
// A runic (Elder Futhark) fingerprint of the key is shown so two peers can
// visually confirm they share the same PSK before trusting the channel.
#pragma once
#include "../../core/Module.h"

class MessengerModule : public Module {
public:
    const char* name() const override { return "Rune Messenger"; }
    Category    category() const override { return Category::LoRa; }
    const char* description() const override { return "AES-GCM P2P text over LoRa"; }
    bool        requiresAuthorization() const override { return true; } // transmits

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;
    void exit(Context& ctx) override;

private:
    bool loadKey(Context& ctx);
    void sendText(Context& ctx, const String& text);
    void drawFingerprint(Context& ctx);
    void drawLog(Context& ctx);

    uint8_t key_[32];
    bool    haveKey_ = false;
    uint32_t seq_ = 0;
    String  log_[6];
    int     logCount_ = 0;
    void    pushLog(const String& s);
};
