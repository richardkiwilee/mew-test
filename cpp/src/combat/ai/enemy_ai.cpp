#include "pch.h"
#include "combat/ai/enemy_ai.h"

#include "combat/core/battle_context.h"

namespace mew::combat {

EntityId EnemyAi::pick_target(EntityId unit_id) const {
    const UnitRuntime* me = ctx_.unit(unit_id);
    if (!me) return kInvalidEntity;
    // 最近存活玩家猫
    EntityId best = kInvalidEntity;
    int32_t best_dist = std::numeric_limits<int32_t>::max();
    for (const auto& u : ctx_.units()) {
        if (u.team != Team::PLAYER || !u.is_alive()) continue;
        const int32_t d = GridMap::manhattan(me->pos, u.pos);
        if (d < best_dist || (d == best_dist && u.entity_id < best)) {
            best_dist = d;
            best = u.entity_id;
        }
    }
    if (best != kInvalidEntity) return best;
    // 无存活目标：攻击最近倒地尸体（摧毁尸体，DD-007）
    for (const auto& u : ctx_.units()) {
        if (u.team != Team::PLAYER || !u.is_down()) continue;
        const int32_t d = GridMap::manhattan(me->pos, u.pos);
        if (d < best_dist || (d == best_dist && u.entity_id < best)) {
            best_dist = d;
            best = u.entity_id;
        }
    }
    return best;
}

uint32_t EnemyAi::pick_ability(EntityId unit_id, EntityId target) const {
    const UnitRuntime* me = ctx_.unit(unit_id);
    const UnitRuntime* t = ctx_.unit(target);
    if (!me || !t) return 0;
    const UnitDef& def = ctx_.def_of(unit_id);
    const int32_t dist = GridMap::manhattan(me->pos, t->pos);
    for (uint32_t aid : def.ability_ids) {
        const AbilityDef* ab = ctx_.catalog().ability_def(aid);
        if (!ab) continue;
        if (me->mana < ab->mana_cost) continue;
        if (dist < ab->min_range || dist > ab->max_range) continue;
        if (ab->needs_los && !ctx_.grid().has_line_of_sight(me->pos, t->pos)) continue;
        if (ab->target_policy == AbilityTargetPolicy::EnemyUnit && t->team == me->team) continue;
        return aid;
    }
    return 0;
}

bool EnemyAi::act_for(EntityId unit_id) {
    UnitRuntime* me = ctx_.unit(unit_id);
    if (!me || !me->is_alive() || ctx_.current_unit() != unit_id) return false;

    const EntityId target = pick_target(unit_id);
    if (target == kInvalidEntity) {
        ctx_.log().trace("AI", "AiNoTarget", std::format("entity={}", unit_id));
        return false;
    }
    const UnitRuntime* t = ctx_.unit(target);
    const int32_t dist = GridMap::manhattan(me->pos, t->pos);

    // 1. 施法（远程技能优先）
    const uint32_t ability = pick_ability(unit_id, target);
    if (ability != 0) {
        const auto res = ctx_.try_cast_ability(unit_id, ability, target, t->pos);
        if (res.ok) {
            ctx_.log().trace("AI", "AiCast",
                             std::format("entity={} casts {} at {}", unit_id, ability, target));
            return true;
        }
    }

    // 2. 近战普攻（8 邻域含斜角）
    if (dist == 1 && me->action_points[static_cast<size_t>(ActionSlot::BASIC_ATTACK) - 1] >= 1
        && GridMap::adjacent8(me->pos, t->pos)) {
        const auto res = ctx_.try_attack(unit_id, target);
        if (res.ok) {
            ctx_.log().trace("AI", "AiAttack",
                             std::format("entity={} attacks {}", unit_id, target));
            return true;
        }
    }

    // 3. 移动逼近：取可达格中曼哈顿距离最小者（平手取扫描序更早，确定性）
    if (me->action_points[static_cast<size_t>(ActionSlot::MOVE) - 1] >= 1) {
        const auto reachable = ctx_.reachable_tiles(unit_id, /*ignore_occupants=*/false);
        Vec2i best_pos = me->pos;
        int32_t best_d = std::numeric_limits<int32_t>::max();
        for (Vec2i p : reachable) {
            if (p == me->pos) continue;
            const int32_t d = GridMap::manhattan(p, t->pos);
            if (d < best_d) {
                best_d = d;
                best_pos = p;
            }
        }
        if (best_pos != me->pos) {
            const auto path = ctx_.path_to(unit_id, best_pos);
            const auto res = ctx_.try_move(unit_id, path);
            if (res.ok) {
                ctx_.log().trace("AI", "AiMove",
                                 std::format("entity={} moves to ({},{})", unit_id, best_pos.x,
                                             best_pos.y));
                return true;
            }
        }
    }
    return false;
}

}  // namespace mew::combat
