# ============================================================================
# mew_battle GDExtension — Godot 4.6 调用 combat_core 的 DLL
# 依赖 godot-cpp（本机 D:/godot/godot-cpp-10.0.0-rc1，可用 -DGODOT_CPP_ROOT 覆盖）
# ============================================================================
cmake_minimum_required(VERSION 3.25)
project(mew_gdext LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED GODOT_CPP_ROOT)
    set(GODOT_CPP_ROOT "D:/godot/godot-cpp-10.0.0-rc1" CACHE PATH "godot-cpp source root")
endif()

# 构建 godot-cpp（godot::cpp 静态库；gdextension/extension_api.json 已更新为 4.6.2）
add_subdirectory("${GODOT_CPP_ROOT}" godot-cpp-build)

add_library(mew_battle SHARED
    src/mew_gdextension.cpp
    src/battle_controller.cpp
)
target_include_directories(mew_battle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(mew_battle PRIVATE combat_core godot::cpp)
set_target_properties(mew_battle PROPERTIES PREFIX "" OUTPUT_NAME "mew_battle")
target_compile_definitions(mew_battle PRIVATE GODOT_EXTENSION)

if(MSVC)
    target_compile_options(mew_battle PRIVATE /W3 /utf-8)
endif()
