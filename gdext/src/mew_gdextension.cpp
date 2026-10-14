// GDExtension 入口：注册 BattleController 类
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "battle_controller.h"

using namespace godot;

namespace {

void initialize_gdextension(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    GDREGISTER_CLASS(mew_gdext::BattleController);
}

void uninitialize_gdextension(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}

}  // namespace

extern "C" {

// .gdextension 文件中的 entry_symbol 必须与此一致
GDExtensionBool GDE_EXPORT mew_battle_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    GDExtensionClassLibraryPtr p_library, GDExtensionInitialization* r_initialization) {
    GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
    init_obj.register_initializer(initialize_gdextension);
    init_obj.register_terminator(uninitialize_gdextension);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}

}  // extern "C"
