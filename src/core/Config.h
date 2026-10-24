// Config.h — YAML-lite loader. Reads /huginn/config.yaml into a flat map keyed
// by dotted path (e.g. "radio.frequency_mhz"). See docs/ARCHITECTURE.md for the
// supported subset. Typed getters fall back to a default if the key is absent.
#pragma once
#include <Arduino.h>
#include <map>
#include <vector>

class SdCard;

class Config {
public:
    // Parse the config file; returns false if it can't be read (defaults then
    // apply to every getter).
    bool load(SdCard& sd, const char* path = "/huginn/config.yaml");

    bool     has(const char* key) const;
    String   getString(const char* key, const String& def = "") const;
    long     getInt(const char* key, long def = 0) const;
    float    getFloat(const char* key, float def = 0.f) const;
    bool     getBool(const char* key, bool def = false) const;

    // Sequence values ("- item" lists) under a key.
    std::vector<String> getList(const char* key) const;

    // Set/override at runtime (e.g. after an on-device prompt). Not persisted.
    void set(const char* key, const String& value);

private:
    std::map<String, String>              scalars_;
    std::map<String, std::vector<String>> lists_;
};
