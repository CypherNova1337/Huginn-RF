// SdCard.h — mounts the microSD over SPI and offers path/rotation helpers.
#pragma once
#include <Arduino.h>
#include <FS.h>

class SdCard {
public:
    bool begin();
    bool mounted() const { return mounted_; }

    bool exists(const char* path);
    bool mkdirs(const char* path);          // create parent dirs as needed

    // Open a capture file under /huginn/captures/, creating a timestamped name
    // from `prefix` and `ext` (e.g. "wardrive" -> wardrive_000123.csv).
    File openCapture(const char* prefix, const char* ext);

    fs::FS& fs();

private:
    bool mounted_ = false;
};
