#pragma once
// ============================================================================
// RngService — 确定性随机服务（设计文档 §13）
// ============================================================================

#include <string>
#include <vector>

#include "combat/core/basic_types.h"
// ============================================================================
// - splitmix64 实现；同一 seed + 同一调用序列 => 同一结果序列；
// - 战斗内一切随机（命中/暴击/概率触发/随机目标/tie-break/AI 决策）必须走这里，
//   禁止任何模块自行创建 RNG；
// - 每次调用记录 rng_call_id / 原因 / 参数 / 结果，供战斗日志与回放审计；
// - 展示层随机（UI 抖动等）必须用独立的 presentation RNG，不得消耗战斗序列。
// ============================================================================

namespace mew::combat {

struct RngCall {
    uint32_t call_id = 0;        // 全局递增，从 1 开始
    std::string reason;          // 例: "hit_roll" / "crit_roll" / "initiative_tie_break"
    int64_t param_a = 0;         // 参数（如随机上界）
    int64_t param_b = 0;
    int64_t result = 0;          // 解释后的结果
};

class RngService {
public:
    explicit RngService(uint64_t seed) : seed_(seed), state_(seed) {}

    uint64_t seed() const { return seed_; }
    const std::vector<RngCall>& calls() const { return calls_; }

    // [0, upper) 均匀随机；upper 必须 > 0
    uint32_t next_u32(uint32_t upper, std::string_view reason);

    // [min, max] 闭区间
    int32_t next_range(int32_t min, int32_t max, std::string_view reason);

    // 概率判定：chance_bp 单位基点，0..10000；<= roll 视为成功
    // 概率 0/100% 按统一规则优化为不消耗随机数（全项目一致，保证回放序列稳定）。
    bool roll_chance(int32_t chance_bp, std::string_view reason);

    // 生成 [0, 2^32) 的稳定 tie-break 值
    uint32_t next_tie_break(std::string_view reason);

private:
    uint64_t next_raw();
    void record(std::string_view reason, int64_t pa, int64_t pb, int64_t result);

    uint64_t seed_ = 0;
    uint64_t state_ = 0;
    uint32_t call_seq_ = 0;
    std::vector<RngCall> calls_;
};

}  // namespace mew::combat
