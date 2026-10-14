#pragma once
// ============================================================================
// TargetSelector — 受控目标选择器（设计文档 §11.1）
// 返回稳定顺序（默认 entity_id 升序）的实体列表；随机选择走 RngService 并记录。
// ============================================================================

#include <vector>

#include "combat/domain/passive_def.h"

namespace mew::combat {

class BattleContext;
struct CombatEvent;

struct TargetContext {
    const CombatEvent& event;
    EntityId owner;
};

class TargetSelector {
public:
    explicit TargetSelector(BattleContext& ctx) : ctx_(ctx) {}

    // 无合法目标时返回空列表（失败策略由调度器记录）
    std::vector<EntityId> resolve(const EffectDef& def, const TargetContext& tc) const;

private:
    std::vector<EntityId> units_of_team(Team team) const;  // entity_id 升序
    std::vector<EntityId> all_alive_units() const;

    BattleContext& ctx_;
};

}  // namespace mew::combat
