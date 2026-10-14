#pragma once
// ============================================================================
// 预编译头：战斗核心常用标准库与全项目共享头。
// CMake 通过 target_precompile_headers(combat_core PRIVATE src/pch.h) 启用，
// 大幅降低全量编译时间（目标：与 Mewgenics 45-60s 全量编译体验一致甚至更快）。
// ============================================================================

// 常用标准库
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <deque>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

// Python 代码生成器产出的枚举反射（单一数据源：tools/codegen/schema.py）
#include "generated/reflection_gen.h"

// 项目通用基础类型与工具
#include "combat/core/basic_types.h"
#include "combat/core/vec2i.h"
