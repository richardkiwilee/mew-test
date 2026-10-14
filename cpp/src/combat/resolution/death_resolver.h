#pragma once
// ============================================================================
// DeathResolver — 统一死亡处理（设计文档 §12 / DD-007）
// ============================================================================

#include "combat/core/basic_types.h"
// ============================================================================
// Phase 2（最小版）：
//   ALIVE 且 hp<=0 → DOWNED（玩家单位）或 DEAD（敌人无尸体韧性）；
//   DOWNED 受攻击 → 尸体韧性 −1，耗尽 → DEAD（不可复活）。
//   Phase 5 扩展：BeforeUnitDeath 干预、复活、召唤、击杀归属与奖励。
// ============================================================================

namespace mew::combat {

class BattleContext;

class DeathResolver {
public:
    explicit DeathResolver(BattleContext& ctx) : ctx_(ctx) {}

    // 处理全部死亡候选（任何状态变化后调用）：downed → dead 两级
    void process_deaths(EntityId last_damage_source);

    // 倒地单位承受一次伤害性攻击：尸体韧性 −1（与伤害数值无关，wiki）
    void damage_corpse(EntityId id, EntityId attacker);

    // 胜负判定；结束战斗时置 battle_over/winner
    void check_battle_end();

private:
    BattleContext& ctx_;
};

}  // namespace mew::combat
