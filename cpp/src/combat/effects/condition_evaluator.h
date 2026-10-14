#pragma once
// ============================================================================
// ConditionEvaluator — 受控条件树求值（设计文档 §3.2 / §5.1 第 6 步）
// 只接受枚举化的条件操作，禁止任意脚本；每次判定返回原因（日志可追踪）。
// ============================================================================

#include <string>

#include "combat/domain/passive_def.h"

namespace mew::combat {

class BattleContext;
struct CombatEvent;

struct EvalContext {
    const CombatEvent& event;
    EntityId owner;             // 效果所有者单位
};

class ConditionEvaluator {
public:
    explicit ConditionEvaluator(BattleContext& ctx) : ctx_(ctx) {}

    // 求值；不满足时 why_not 说明原因（用于日志）
    bool evaluate(const ConditionNode& node, const EvalContext& ec, std::string* why_not) const;

private:
    BattleContext& ctx_;
};

}  // namespace mew::combat
