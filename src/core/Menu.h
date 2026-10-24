// Menu.h — two-level menu: categories -> modules. Runs the selected module's
// enter/loop/exit cycle, and enforces the authorized-use gate for offensive
// modules before handing them control.
#pragma once
#include "Module.h"

class Menu {
public:
    // Blocks forever, driving the UI. Called once from loop()/App.
    void run(Context& ctx);

private:
    void runCategoryList(Context& ctx);
    void runModuleList(Context& ctx, Category cat);
    void runModule(Context& ctx, Module* m);

    // Returns true if the user may proceed (already authorized, or accepts the
    // on-device prompt, which also sets ctx.authorized for the session).
    bool ensureAuthorized(Context& ctx, Module* m);
};
