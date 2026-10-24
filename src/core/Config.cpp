#include "Config.h"
#include "SdCard.h"
#include <FS.h>

static String trimR(const String& s) {
    int end = s.length();
    while (end > 0 && (s[end - 1] == ' ' || s[end - 1] == '\r' || s[end - 1] == '\t')) end--;
    return s.substring(0, end);
}

static void stripComment(String& s) {
    // Strip a trailing " # comment" but keep '#' inside quotes.
    bool inq = false;
    for (int i = 0; i < (int)s.length(); ++i) {
        if (s[i] == '"') inq = !inq;
        else if (s[i] == '#' && !inq) { s = s.substring(0, i); return; }
    }
}

static String unquote(String v) {
    v.trim();
    if (v.length() >= 2 && v[0] == '"' && v[v.length() - 1] == '"')
        v = v.substring(1, v.length() - 1);
    return v;
}

bool Config::load(SdCard& sd, const char* path) {
    scalars_.clear();
    lists_.clear();
    if (!sd.mounted()) return false;
    File f = sd.fs().open(path, FILE_READ);
    if (!f) return false;

    // Track the key prefix at each indent depth (2 spaces == one level).
    String stack[12];
    String lastKeyAtDepth[12];
    int    prevDepth = 0;

    while (f.available()) {
        String raw = f.readStringUntil('\n');
        stripComment(raw);
        String line = trimR(raw);
        if (line.length() == 0) continue;

        // indentation
        int indent = 0;
        while (indent < (int)line.length() && line[indent] == ' ') indent++;
        int depth = indent / 2;
        if (depth > 11) depth = 11;
        String content = line.substring(indent);

        if (content.startsWith("- ")) {
            // Sequence item belongs to the key most recently seen one level up.
            String val = unquote(content.substring(2));
            String owner;
            for (int d = depth - 1; d >= 0; --d) {
                if (lastKeyAtDepth[d].length()) { owner = lastKeyAtDepth[d]; break; }
            }
            if (owner.length()) lists_[owner].push_back(val);
            continue;
        }

        int colon = content.indexOf(':');
        if (colon < 0) continue;
        String key = trimR(content.substring(0, colon));
        String val = content.substring(colon + 1);
        val.trim();

        // Build dotted path from the current stack up to this depth.
        String prefix;
        for (int d = 0; d < depth; ++d)
            if (stack[d].length()) prefix += stack[d] + ".";
        String full = prefix + key;

        lastKeyAtDepth[depth] = full;
        for (int d = depth + 1; d < 12; ++d) lastKeyAtDepth[d] = "";

        if (val.length() == 0) {
            // A nested map opens here.
            stack[depth] = key;
            for (int d = depth + 1; d < 12; ++d) stack[d] = "";
        } else {
            scalars_[full] = unquote(val);
        }
        prevDepth = depth;
    }
    f.close();
    return true;
}

bool Config::has(const char* key) const {
    return scalars_.count(String(key)) || lists_.count(String(key));
}

String Config::getString(const char* key, const String& def) const {
    auto it = scalars_.find(String(key));
    return it == scalars_.end() ? def : it->second;
}

long Config::getInt(const char* key, long def) const {
    auto it = scalars_.find(String(key));
    return it == scalars_.end() ? def : strtol(it->second.c_str(), nullptr, 0);
}

float Config::getFloat(const char* key, float def) const {
    auto it = scalars_.find(String(key));
    return it == scalars_.end() ? def : atof(it->second.c_str());
}

bool Config::getBool(const char* key, bool def) const {
    auto it = scalars_.find(String(key));
    if (it == scalars_.end()) return def;
    String v = it->second; v.toLowerCase();
    return v == "true" || v == "1" || v == "yes" || v == "on";
}

std::vector<String> Config::getList(const char* key) const {
    auto it = lists_.find(String(key));
    return it == lists_.end() ? std::vector<String>{} : it->second;
}

void Config::set(const char* key, const String& value) {
    scalars_[String(key)] = value;
}
