#pragma once
// ============================================================================
// CombatEvent — 战斗事件（设计文档 §4.4）
// 命名表达"已发生事实"或"即将发生的机会"：
//   BEFORE（可干预）→ COMMIT（已提交）→ AFTER（只能响应结果）
// 因果链：parent_event_id / depth / root_action_id（DD-009 防无限递归）。
// ============================================================================

#include <string>

#include "combat/core/basic_types.h"
#include "combat/core/vec2i.h"
#include "generated/reflection_gen.h"

namespace mew::combat {

struct CombatEvent {
    EventId event_id = kInvalidEvent;
    EventType type = EventType::BattleStarted;
    EventPhase phase = EventPhase::COMMIT;   // 本事件的阶段语义
    EntityId source = kInvalidEntity;        // 发起者（可为空）
    EntityId target = kInvalidEntity;        // 主要目标（可为空）
    int64_t amount = 0;                      // 数值（伤害/资源量/层数等）
    ResourceType resource = ResourceType::HP; // ResourceSpent/Gained 等
    ActionId action_id = kInvalidAction;
    uint32_t ability_id = 0;                 // 相关技能定义（0 = 无）
    Vec2i position{-1, -1};                  // 相关格子（可为空）
    SequenceId seq = 0;                      // 全局事件序号（发布时分配）
    int32_t depth = 0;                       // 因果链深度
    EventId parent_event_id = kInvalidEvent;
    ActionId root_action_id = kInvalidAction;
    std::vector<std::string> tags;           // 语义标签（如 "reflect" / "forced_move"）
};

}  // namespace mew::combat
