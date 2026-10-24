#include "Messenger.h"
#include "../../core/ModuleRegistry.h"
#include "../../core/Display.h"
#include "../../core/Input.h"
#include "../../core/Config.h"
#include "../../core/Logger.h"
#include "../../hal/Lora.h"
#include <AES.h>
#include <GCM.h>
#include <esp_random.h>

// 16 Elder Futhark rune names, one per key-fingerprint nibble. Rendering the
// names (rather than glyphs) keeps it legible on the built-in bitmap font while
// still giving a memorable, order-sensitive phrase to compare between peers.
static const char* kRunes[16] = {
    "Fehu","Uruz","Thurisaz","Ansuz","Raido","Kenaz","Gebo","Wunjo",
    "Hagalaz","Nauthiz","Isa","Jera","Eihwaz","Perthro","Algiz","Sowilo"
};

static int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool MessengerModule::loadKey(Context& ctx) {
    String hex = ctx.config->getString("messenger.psk_hex", "");
    if (hex.length() != 64) {
        // Prompt once on-device if not set in config.
        String entered;
        if (!ctx.input->readLine("PSK (64 hex) or Esc:", entered, true)) return false;
        hex = entered;
    }
    if (hex.length() != 64) return false;
    for (int i = 0; i < 32; ++i) {
        int hi = hexNibble(hex[i * 2]), lo = hexNibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        key_[i] = (hi << 4) | lo;
    }
    return true;
}

// A short runic fingerprint: SHA-like fold of the key into 6 nibbles -> 6 runes.
void MessengerModule::drawFingerprint(Context& ctx) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < 32; ++i) { h ^= key_[i]; h *= 16777619u; }
    ctx.display->line(0, "Key fingerprint (compare):", 0x8410);
    char buf[64] = {0};
    for (int i = 0; i < 6; ++i) {
        int nib = (h >> (i * 4)) & 0xF;
        strncat(buf, kRunes[nib], sizeof(buf) - strlen(buf) - 1);
        if (i < 5) strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
    }
    ctx.display->line(1, buf, 0xFFE0);
}

void MessengerModule::pushLog(const String& s) {
    if (logCount_ < 6) log_[logCount_++] = s;
    else { for (int i = 0; i < 5; ++i) log_[i] = log_[i + 1]; log_[5] = s; }
}

void MessengerModule::drawLog(Context& ctx) {
    for (int i = 0; i < logCount_; ++i)
        ctx.display->line(3 + i, log_[i].c_str(), 0xFFFF);
}

void MessengerModule::sendText(Context& ctx, const String& text) {
    if (!haveKey_ || !ctx.lora) return;

    uint8_t frame[12 + 200 + 16];
    uint8_t* nonce = frame;
    esp_fill_random(nonce, 12);

    size_t plen = min((size_t)text.length(), (size_t)200);
    uint8_t* ct  = frame + 12;
    uint8_t* tag = frame + 12 + plen;

    GCM<AES256> gcm;
    gcm.setKey(key_, 32);
    gcm.setIV(nonce, 12);
    gcm.encrypt(ct, (const uint8_t*)text.c_str(), plen);
    gcm.computeTag(tag, 16);

    size_t total = 12 + plen + 16;
    if (ctx.lora->transmit(frame, total)) {
        if (ctx.log) ctx.log->tx("messenger", "sent %u bytes (seq %lu)",
                                 (unsigned)total, (unsigned long)seq_++);
        pushLog(String("> ") + text);
    } else {
        pushLog("! TX failed");
    }
}

void MessengerModule::enter(Context& ctx) {
    ctx.display->clear();
    ctx.display->header("Rune Messenger");
    haveKey_ = loadKey(ctx);
    logCount_ = 0;
    if (!haveKey_) { pushLog("No/invalid PSK"); return; }
    if (ctx.lora) ctx.lora->startReceive();
    drawFingerprint(ctx);
}

bool MessengerModule::loop(Context& ctx) {
    if (!haveKey_) {
        ctx.display->footer("Esc=back (set messenger.psk_hex)");
        if (ctx.input->poll().key == Key::Back) return false;
        delay(20); return true;
    }

    // Receive & decrypt.
    Lora::RxPacket pkt;
    if (ctx.lora && ctx.lora->receive(pkt) && pkt.len > 28) {
        const uint8_t* nonce = pkt.data;
        size_t clen = pkt.len - 12 - 16;
        const uint8_t* ct  = pkt.data + 12;
        const uint8_t* tag = pkt.data + 12 + clen;

        uint8_t out[201];
        GCM<AES256> gcm;
        gcm.setKey(key_, 32);
        gcm.setIV(nonce, 12);
        gcm.decrypt(out, ct, clen);
        if (gcm.checkTag(tag, 16)) {
            out[min(clen, (size_t)200)] = 0;
            pushLog(String("< ") + (char*)out);
        } else {
            pushLog("< [bad tag — wrong key?]");
        }
    }

    drawLog(ctx);
    ctx.display->footer("Enter=compose  Esc=back");

    KeyEvent e = ctx.input->poll();
    if (e.key == Key::Back) return false;
    if (e.key == Key::Enter) {
        String msg;
        if (ctx.input->readLine("msg:", msg) && msg.length()) sendText(ctx, msg);
        ctx.display->clear();
        ctx.display->header("Rune Messenger");
        drawFingerprint(ctx);
    }
    delay(5);
    return true;
}

void MessengerModule::exit(Context& ctx) {
    memset(key_, 0, sizeof(key_));   // wipe key from RAM on exit
    haveKey_ = false;
}

REGISTER_MODULE(MessengerModule);
