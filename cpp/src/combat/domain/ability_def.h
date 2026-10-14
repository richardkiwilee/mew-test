#pragma once
// ============================================================================
// AbilityDef — 只读技能定义（设计文档 §7）
// 技能 = 成本 + 范围/目标策略 + 有序 payload 效果列表（step_index 顺序执行）。
// 结算走统一 ActionResolver + EffectRegistry，技能本身不含专属代码分支。
// ============================================================================

#include <string>
#include <vector>

#include "combat/core/basic_types.h"
#include "combat/domain/passive_def.h"   // EffectDef 复用

namespace mew::combat {

// 技能目标策略
enum class AbilityTargetPolicy : uint8_t {
    EnemyUnit,   // 敌方单位
    AllyUnit,    // 友方单位（含自身）
    AnyUnit,
    EmptyTile,   // 空格子（范围伤害等）
    Self,
};

struct AbilityDef {
    uint32_t id = 0;
    std::string display_name;
    std::string description;
    int32_t mana_cost = 0;              // 0 = 无消耗
    int32_t min_range = 1;              // 曼哈顿距离
    int32_t max_range = 1;              // 1 = 近战（8 邻域）
    int32_t radius = 0;                 // 溅射半径（0 = 单体）
    bool needs_los = false;             // 远程技能是否要求视线
    bool spends_action = true;          // 是否消耗基础攻击动作（多数技能替代普攻）
    AbilityTargetPolicy target_policy = AbilityTargetPolicy::EnemyUnit;
    std::vector<EffectDef> payload;     // 按 step_index（定义序）执行的结算步骤
};

}  // namespace mew::combat
