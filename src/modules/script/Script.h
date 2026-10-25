// Script.h — a tiny macro engine. Runs .hsc scripts from the SD card so you can
// chain actions without reflashing. Commands (one per line, # = comment):
//   DELAY <ms>        wait
//   PRINT <text>      show a line on screen
//   LOG <text>        write to the SD log
//   IR <path>         blast every raw code in a Flipper-style .ir file
//   REPEAT <n> ... END  repeat the enclosed block n times (nestable)
// Deliberately small and self-contained; more verbs can be added over time.
#pragma once
#include "../../core/Module.h"
#include <vector>

class ScriptModule : public Module {
public:
    const char* name() const override { return "Scripts"; }
    Category    category() const override { return Category::Tools; }
    const char* description() const override { return "Run .hsc macro scripts from SD"; }
    bool        requiresAuthorization() const override { return false; }

    void enter(Context& ctx) override;
    bool loop(Context& ctx) override;

private:
    enum class View { List, Run };
    View view_ = View::List;

    void listScripts(Context& ctx);
    void run(Context& ctx, const String& path);
    void execIr(Context& ctx, const String& path);
    void pushOut(const String& s);

    std::vector<String> files_;
    int  sel_ = 0;
    String out_[6];
    int  outN_ = 0;
};
