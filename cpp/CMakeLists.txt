# ============================================================================
# mew_combat — Mewgenics 风格回合制战斗核心（静态库）+ 单元测试
# 构建：仓库根目录 build.bat（自动定位 VS2022 自带 CMake/Ninja + vcvars64）
# 要点：Python 反射代码生成（configure 时自动执行）、预编译头（combat_core）
# ============================================================================
cmake_minimum_required(VERSION 3.25)
project(mew_combat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# 1. 反射代码生成（Python -> C++）
#    configure 时执行一次；内容不变时不会触碰输出文件（不触发重编译）。
#    手动重跑：cmake --build build --target mew_codegen
# ---------------------------------------------------------------------------
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(MEW_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/generated")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/codegen/generate.py" --out "${MEW_GEN_DIR}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tools/codegen"
  RESULT_VARIABLE MEW_CODEGEN_RESULT
  OUTPUT_VARIABLE MEW_CODEGEN_OUTPUT
)
if(NOT MEW_CODEGEN_RESULT EQUAL 0)
  message(FATAL_ERROR "reflection codegen failed:\n${MEW_CODEGEN_OUTPUT}")
endif()
add_custom_target(mew_codegen
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/codegen/generate.py" --out "${MEW_GEN_DIR}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tools/codegen"
  COMMENT "Re-running reflection codegen (schema.py -> src/generated)"
  VERBATIM)

# ---------------------------------------------------------------------------
# 2. 战斗核心静态库（含预编译头）
# ---------------------------------------------------------------------------
file(GLOB_RECURSE MEW_CORE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/combat/*.cpp"
  "${MEW_GEN_DIR}/*.cpp"
)
add_library(combat_core STATIC ${MEW_CORE_SOURCES})
target_include_directories(combat_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_precompile_headers(combat_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h")

if(MSVC)
  target_compile_options(combat_core PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
  target_compile_definitions(combat_core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_definitions(combat_core PUBLIC MEW_COMBAT_LIB)
endif()

# ---------------------------------------------------------------------------
# 3. 单元测试
# ---------------------------------------------------------------------------
enable_testing()
file(GLOB MEW_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
add_executable(mew_combat_tests ${MEW_TEST_SOURCES})
target_link_libraries(mew_combat_tests PRIVATE combat_core)
if(MSVC)
  target_compile_options(mew_combat_tests PRIVATE /W4 /permissive- /utf-8)
endif()

include(CTest)
add_test(NAME mew_combat_tests COMMAND mew_combat_tests)

# ---------------------------------------------------------------------------
# 4. GDExtension（Godot 调用层，依赖 combat_core）
# ---------------------------------------------------------------------------
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../gdext" gdext-build)
