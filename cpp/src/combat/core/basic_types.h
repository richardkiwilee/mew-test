#pragma once
// ============================================================================
// 基础类型约定
// - 数值：战斗数值一律整数（int32_t 存储 / int64_t 运算），保证跨平台确定性；
//   百分比/倍率用"基点"表示（10000 = 100%），取整规则集中在 utils 中定义。
// - 实体/事件 ID：uint32_t，0 = 无效。实体 ID 在战斗内唯一且稳定，
//   不得用数组索引代表长期实体身份（设计文档 §4.1）。
// ============================================================================

#include <cassert>
#include <cstdint>

namespace mew::combat {

using EntityId = uint32_t;
using EventId = uint32_t;
using ActionId = uint32_t;
using RoundIndex = uint32_t;
using SequenceId = uint32_t;
using InstanceId = uint32_t;

constexpr EntityId kInvalidEntity = 0;
constexpr EventId kInvalidEvent = 0;
constexpr ActionId kInvalidAction = 0;

// 基点常量：百分比/倍率的整数表示
constexpr int32_t kPercentBase = 10000;   // 100%
constexpr int32_t kPercentHalf = 5000;    // 50%

// 常用取整规则：全部经此函数，保证项目内一致
inline int64_t div_round_half_away(int64_t value, int64_t divisor) {
    assert(divisor > 0);
    const int64_t q = value / divisor;
    const int64_t r = value % divisor;
    const int64_t adjust = (r >= (divisor + 1) / 2) ? 1 : 0;
    return q + adjust;
}

// 向下取整（C++ 原生 / 是向零截断，二者对负数不同；wiki 的"向下取整"公式用此）
inline int64_t div_floor(int64_t value, int64_t divisor) {
    assert(divisor > 0);
    const int64_t q = value / divisor;
    const int64_t r = value % divisor;
    return (r < 0) ? q - 1 : q;
}

// 向零截断（wiki 的 trunc 语义，如移动范围公式）
inline int64_t div_trunc(int64_t value, int64_t divisor) {
    assert(divisor > 0);
    return value / divisor;
}

// 基点乘法：value * mul_bp / 10000，半程进位
inline int64_t apply_bp(int64_t value, int64_t mul_bp) {
    return div_round_half_away(value * mul_bp, kPercentBase);
}

inline int32_t clamp_to(int64_t v, int32_t lo, int32_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return static_cast<int32_t>(v);
}

}  // namespace mew::combat
