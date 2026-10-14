#pragma once
// 网格坐标（战斗核心自持，不依赖引擎；GDExtension 层负责与 Godot Vector2i 互转）

#include <cstddef>
#include <cstdint>
#include <functional>

namespace mew::combat {

struct Vec2i {
    int32_t x = 0;
    int32_t y = 0;

    constexpr bool operator==(const Vec2i& o) const { return x == o.x && y == o.y; }
    constexpr bool operator!=(const Vec2i& o) const { return !(*this == o); }
    constexpr Vec2i operator+(const Vec2i& o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2i operator-(const Vec2i& o) const { return {x - o.x, y - o.y}; }
};

// 与地图尺寸对齐的可哈希键（unordered 容器用）
struct Vec2iHash {
    size_t operator()(const Vec2i& v) const noexcept {
        return std::hash<uint64_t>{}((static_cast<uint64_t>(static_cast<uint32_t>(v.x)) << 32)
                                     | static_cast<uint32_t>(v.y));
    }
};

}  // namespace mew::combat
