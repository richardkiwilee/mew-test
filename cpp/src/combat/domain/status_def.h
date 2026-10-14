#pragma once
// ============================================================================
// StatusDef / StatusInstance — 状态定义与运行时实例（设计文档 §10）
// 叠层策略/计时单位/tick 时机全部枚举化，杜绝 off-by-one（DD-008）。
// ============================================================================

#include <string>
#include <vector>

#include "combat/core/basic_types.h"
#include "combat/domain/passive_def.h"   // StatModifierDef / EffectDef 复用
#include "generated/reflection_gen.h"

namespace mew::combat {

struct StatusDef {
    uint32_t id = 0;
    std::string display_name;
    std::string description;
    std::vector<std::string> tags;
    StackPolicy stack_policy = StackPolicy::STACK_AND_REFRESH;
    int32_t max_stacks = 1;              // 0 = 无上限
    DurationUnit duration_unit = DurationUnit::ACTOR_TURNS;
    int32_t duration_value = 0;          // 0 = 永久（依赖 duration_unit）
    TickTiming tick_timing = TickTiming::EVENT_DRIVEN;
    bool tick_before_decay = true;       // DD-008：先结算 tick 再减时长
    bool removable = true;               // 驱散/倒地清除
    // tick 语义（wiki 对齐）
    bool decay_stacks_on_tick = false;   // Burn：tick 后层数 −1
    int32_t stack_delta_on_tick = 0;     // Bleed：tick 后层数 +1
    bool expire_when_stacks_zero = true; // 层数归零即过期
    bool blocks_turn = false;            // Stun 类：跳过持有者回合（DD-019）
    std::vector<StatModifierDef> stat_modifiers;  // 状态附带的属性修正
    std::vector<EffectDef> effects;               // tick 触发的效果（IMMEDIATE 施加时执行）
};

struct StatusInstance {
    InstanceId instance_id = 0;
    uint32_t status_def_id = 0;
    EntityId source_entity = kInvalidEntity;
    EntityId target_entity = kInvalidEntity;
    int32_t stacks = 1;
    int32_t remaining_duration = 0;      // 按 duration_unit 计
    uint32_t applied_seq = 0;
    uint32_t applied_turn_seq = 0;       // 持有者施加时的回合序号（DD-008：施加回合不计时长）
};

}  // namespace mew::combat
