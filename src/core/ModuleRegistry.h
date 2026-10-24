// ModuleRegistry.h — modules self-register here at startup so the menu can
// enumerate them by category without the core knowing their types.
#pragma once
#include "Module.h"
#include <vector>

class ModuleRegistry {
public:
    static ModuleRegistry& instance();

    void add(Module* m);
    const std::vector<Module*>& all() const { return modules_; }

    // Modules in a given category, in registration order.
    std::vector<Module*> byCategory(Category c) const;

private:
    std::vector<Module*> modules_;
};

// Helper that registers a module instance before setup() runs.
struct ModuleRegistrar {
    explicit ModuleRegistrar(Module* m) { ModuleRegistry::instance().add(m); }
};

// Place at file scope in a module's .cpp:
//   REGISTER_MODULE(WardriveModule);
#define REGISTER_MODULE(TYPE) \
    static TYPE s_##TYPE##_instance; \
    static ModuleRegistrar s_##TYPE##_registrar(&s_##TYPE##_instance)
