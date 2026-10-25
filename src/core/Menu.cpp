#include "Menu.h"
#include "ModuleRegistry.h"
#include "Display.h"
#include "Input.h"
#include "Config.h"
#include "Logger.h"
#include <vector>

void Menu::run(Context& ctx) {
    for (;;) runCategoryList(ctx);
}

void Menu::runCategoryList(Context& ctx) {
    // Only show categories that actually have at least one module.
    std::vector<Category> cats;
    std::vector<const char*> labels;
    for (int i = 0; i < (int)Category::_count; ++i) {
        Category c = (Category)i;
        if (!ModuleRegistry::instance().byCategory(c).empty()) {
            cats.push_back(c);
            labels.push_back(categoryName(c));
        }
    }

    int sel = 0;
    for (;;) {
        ctx.display->list(labels.data(), labels.size(), sel,
                          "Huginn-RF", "Up/Down  Enter=open");
        ctx.display->present();
        KeyEvent e = ctx.input->poll();
        switch (e.key) {
            case Key::Up:    sel = (sel + labels.size() - 1) % labels.size(); break;
            case Key::Down:  sel = (sel + 1) % labels.size(); break;
            case Key::Enter: runModuleList(ctx, cats[sel]); break;
            default: break;
        }
        delay(10);
    }
}

void Menu::runModuleList(Context& ctx, Category cat) {
    auto mods = ModuleRegistry::instance().byCategory(cat);
    std::vector<const char*> labels;
    for (auto* m : mods) labels.push_back(m->name());

    int sel = 0;
    for (;;) {
        ctx.display->list(labels.data(), labels.size(), sel,
                          categoryName(cat), "Enter=run  Esc=back");
        // Footer: description of the highlighted module.
        ctx.display->footer(mods[sel]->description());
        ctx.display->present();
        KeyEvent e = ctx.input->poll();
        switch (e.key) {
            case Key::Up:    sel = (sel + labels.size() - 1) % labels.size(); break;
            case Key::Down:  sel = (sel + 1) % labels.size(); break;
            case Key::Back:  return;
            case Key::Enter:
                if (ensureAuthorized(ctx, mods[sel])) runModule(ctx, mods[sel]);
                break;
            default: break;
        }
        delay(10);
    }
}

bool Menu::ensureAuthorized(Context& ctx, Module* m) {
    if (!m->requiresAuthorization()) return true;
    if (ctx.authorized) return true;

    // Draw the acknowledgement prompt. 'y' accepts for this session, anything
    // else backs out. This is a guardrail, not legal cover (see docs/LEGAL.md).
    ctx.display->clear();
    ctx.display->header("Authorized use");
    ctx.display->line(0, "This module transmits.", 0xFFE0);
    ctx.display->line(1, "Use only on gear you own", 0xFFFF);
    ctx.display->line(2, "or have written permission", 0xFFFF);
    ctx.display->line(3, "to test.", 0xFFFF);
    ctx.display->line(5, "Press Y to accept, Esc=no", 0x07E0);
    ctx.display->present();
    for (;;) {
        KeyEvent e = ctx.input->poll();
        if (e.key == Key::Back) return false;
        if (e.key == Key::Char && (e.ch == 'y' || e.ch == 'Y')) {
            ctx.authorized = true;
            if (ctx.log) ctx.log->info("Authorized-use accepted on-device");
            return true;
        }
        delay(10);
    }
}

void Menu::runModule(Context& ctx, Module* m) {
    if (ctx.log) ctx.log->info("Enter module: %s", m->name());
    m->enter(ctx);
    ctx.display->present();                 // show whatever enter() drew
    for (;;) {
        bool cont = m->loop(ctx);
        ctx.display->present();             // push this frame (flicker-free)
        if (!cont) break;
        delay(2);
    }
    m->exit(ctx);
    if (ctx.log) ctx.log->info("Exit module: %s", m->name());
}
