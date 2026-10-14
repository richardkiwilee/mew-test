#pragma once
// ============================================================================
// EnemyAi — 敌方 AI（设计文档 §19 阶段 5）
// 只产出"行动意图"并经 ActionResolver 验证执行，绝不直接修改世界状态。
// 决策确定性（最近敌人/扫描序平手规则），保证重放一致（DD-014）。
// ============================================================================

#include "combat/core/basic_types.h"

namespace mew::combat {

class BattleContext;

class EnemyAi {
public:
    explicit EnemyAi(BattleContext& ctx) : ctx_(ctx) {}

    // 为当前单位执行一步 AI 行动（移动/普攻/施法），返回是否执行了行动
    bool act_for(EntityId unit_id);

private:
    // 最近玩家猫（优先存活；无存活则选最近倒地尸体；无则 kInvalidEntity）
    EntityId pick_target(EntityId unit_id) const;
    // 可施放技能：有法力、范围内、有目标（返回 0 = 无）
    uint32_t pick_ability(EntityId unit_id, EntityId target) const;

    BattleContext& ctx_;
};

}  // namespace mew::combat
