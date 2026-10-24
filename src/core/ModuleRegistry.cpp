#include "ModuleRegistry.h"

const char* categoryName(Category c) {
    switch (c) {
        case Category::WiFi:      return "WiFi";
        case Category::Bluetooth: return "Bluetooth";
        case Category::SubGHz:    return "Sub-GHz";
        case Category::LoRa:      return "LoRa";
        case Category::IR:        return "Infrared";
        case Category::RFID:      return "RFID/NFC";
        case Category::USB:       return "USB";
        case Category::Tools:     return "Tools";
        default:                  return "?";
    }
}

ModuleRegistry& ModuleRegistry::instance() {
    static ModuleRegistry reg;
    return reg;
}

void ModuleRegistry::add(Module* m) {
    if (m) modules_.push_back(m);
}

std::vector<Module*> ModuleRegistry::byCategory(Category c) const {
    std::vector<Module*> out;
    for (auto* m : modules_) {
        if (m->category() == c) out.push_back(m);
    }
    return out;
}
