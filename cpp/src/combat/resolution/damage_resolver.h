#pragma once
// ============================================================================
// DamageResolver — 统一伤害管线（设计文档 §8 / wiki: Damage Mechanics）
// ============================================================================

#include <string>

#include "combat/core/basic_types.h"
#include "generated/reflection_gen.h"
// ============================================================================
// 一切伤害来源（普攻、技能、地形、反伤）必须走这里。
// 管线（DD-005）：Base → 减伤(Brace/Petrify) → 背刺 +25%(物理) → 暴击 ×2
//               → 护盾(Holy Shield 每实例 1 次 → Shield) → HP。
// Phase 2：减伤/护盾为 0（Phase 4 状态接入）；暴击已实现（DD-006：魔法不暴击）。
// ============================================================================

namespace mew::combat {

class BattleContext;

struct DamageContext {
    EntityId source = kInvalidEntity;   // 0 = 环境伤害（地形等）
    EntityId target = kInvalidEntity;
    DamageType type = DamageType::PHYSICAL;
    int32_t base_amount = 0;
    bool can_backstab = false;          // 来源是否可能背刺（检测 Phase 6 接朝向）
    std::string reason;                 // 如 "basic_attack" / "burn"
    std::vector<std::string> event_tags; // 伤害事件携带的语义标签（"reflect" 等）
};

struct DamageResult {
    int32_t base_amount = 0;            // 输入基础量
    int32_t mitigated_amount = 0;       // 减伤后
    int32_t after_backstab = 0;
    int32_t final_amount = 0;           // 暴击后（护盾前）
    bool crit = false;
    bool backstab = false;
    int32_t shield_absorbed = 0;        // Phase 4+ 护盾吸收
    int32_t hp_lost = 0;                // 实际扣血
    bool target_downed = false;         // 本次把目标击倒（或击倒状态继续受损）
    bool target_died = false;           // 本次把目标打死（尸体韧性耗尽）
};

class DamageResolver {
public:
    explicit DamageResolver(BattleContext& ctx) : ctx_(ctx) {}

    // 完整管线；死亡候选由调用方随后经 DeathResolver::process_deaths 处理
    DamageResult deal_damage(DamageContext dmg);

private:
    BattleContext& ctx_;
};

}  // namespace mew::combat
