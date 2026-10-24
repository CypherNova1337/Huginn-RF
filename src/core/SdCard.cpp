#include "SdCard.h"
#include "../hal/board.h"
#include <SD.h>
#include <SPI.h>

static SPIClass s_sdSpi(HSPI);

bool SdCard::begin() {
    s_sdSpi.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
    // Conservative clock first; SD on the Cardputer bus can be picky.
    mounted_ = SD.begin(SD_PIN_CS, s_sdSpi, 20000000);
    if (mounted_) {
        if (!SD.exists("/huginn")) SD.mkdir("/huginn");
        if (!SD.exists("/huginn/captures")) SD.mkdir("/huginn/captures");
    }
    return mounted_;
}

fs::FS& SdCard::fs() { return SD; }

bool SdCard::exists(const char* path) { return mounted_ && SD.exists(path); }

bool SdCard::mkdirs(const char* path) {
    if (!mounted_) return false;
    String p(path);
    for (int i = 1; i < (int)p.length(); ++i) {
        if (p[i] == '/') {
            String sub = p.substring(0, i);
            if (!SD.exists(sub)) SD.mkdir(sub);
        }
    }
    if (!SD.exists(p)) SD.mkdir(p);
    return true;
}

File SdCard::openCapture(const char* prefix, const char* ext) {
    if (!mounted_) return File();
    // Find the next free sequence number so we never clobber a prior capture.
    for (uint32_t n = 0; n < 1000000; ++n) {
        char path[96];
        snprintf(path, sizeof(path), "/huginn/captures/%s_%06lu.%s",
                 prefix, (unsigned long)n, ext);
        if (!SD.exists(path)) return SD.open(path, FILE_WRITE);
    }
    return File();
}
