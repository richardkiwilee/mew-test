#include "pch.h"
#include "combat/resolution/action_resolver.h"

#include "combat/core/battle_context.h"

namespace mew::combat {

namespace {

// 从枚举 ActionSlot 取下标（schema.py 值从 1 开始）
constexpr size_t slot_index(ActionSlot s) {
    return static_cast<size_t>(s) - 1;
}

}  // namespace

MoveResult ActionResolver::try_move(EntityId unit_id, const std::vector<Vec2i>& path) {
    MoveResult res;
    UnitRuntime* u = ctx_.unit(unit_id);
    if (!u) {
        res.reject = "unknown_unit";
        return res;
    }
    if (ctx_.current_unit() != unit_id) {
        res.reject = "not_your_turn";
        return res;
    }
    if (!u->is_alive()) {
        res.reject = "unit_not_alive";
        return res;
    }
    if (u->action_points[slot_index(ActionSlot::MOVE)] < 1) {
        res.reject = "no_move_action";
        return res;
    }
    if (path.size() < 2) {
        res.reject = "empty_path";
        return res;
    }
    if (path.front() != u->pos) {
        res.reject = "path_start_mismatch";
        return res;
    }

    // 校验路径：四方向逐格、预算（半步，DD-002）、目标格可进入
    const int32_t budget = ctx_.move_budget_of(unit_id);
    int32_t cost = 0;
    for (size_t i = 1; i < path.size(); ++i) {
        const Vec2i from = path[i - 1];
        const Vec2i to = path[i];
        if (GridMap::manhattan(from, to) != 1) {
            res.reject = "non_adjacent_step";
            return res;
        }
        if (!ctx_.grid().in_bounds(to) || !ctx_.grid().terrain_passable(to)) {
            res.reject = "blocked_tile";
            return res;
        }
        const EntityId occ = ctx_.grid().occupant(to);
        if (occ != kInvalidEntity && occ != unit_id) {
            res.reject = "tile_occupied";
            return res;
        }
        cost += ctx_.grid().move_cost_half_steps(to);
        if (cost > budget) {
            res.reject = "out_of_range";
            return res;
        }
    }

    // ---- 执行：支付成本，逐格移动并发布事件（地形反应订阅 UnitEnteredTile，Phase 6）----
    --u->action_points[slot_index(ActionSlot::MOVE)];
    ctx_.publish_event(
        CombatEvent{.type = EventType::MovementStarted, .phase = EventPhase::BEFORE,
                    .source = unit_id, .amount = static_cast<int64_t>(path.size() - 1)},
        "Action", {f_int("entity", unit_id)},
        std::format("entity={} steps={}", unit_id, path.size() - 1));
    for (size_t i = 1; i < path.size(); ++i) {
        const Vec2i from = path[i - 1];
        const Vec2i to = path[i];
        ctx_.publish_event(
            CombatEvent{.type = EventType::UnitLeftTile, .phase = EventPhase::BEFORE,
                        .source = unit_id, .position = from},
            "Action", {}, std::format("entity={} leaves ({},{})", unit_id, from.x, from.y));
        ctx_.grid_.clear_occupant(from);
        ctx_.grid_.place_unit(unit_id, to);
        u->pos = to;
        ctx_.publish_event(
            CombatEvent{.type = EventType::UnitMoved, .phase = EventPhase::COMMIT,
                        .source = unit_id, .position = to},
            "Action", {}, std::format("entity={} -> ({},{})", unit_id, to.x, to.y));
        ctx_.publish_event(
            CombatEvent{.type = EventType::UnitEnteredTile, .phase = EventPhase::AFTER,
                        .source = unit_id, .position = to},
            "Action", {}, std::format("entity={} enters ({},{})", unit_id, to.x, to.y));
        // 地形规则（设计文档 §11.2：地图只发事件，由地形规则执行伤害/状态）
        ctx_.apply_terrain_on_enter(unit_id, to);
        if (!u->is_alive()) {
            ctx_.log_.append("Action", "MovementCancelled",
                             {f_int("entity", unit_id)},
                             std::format("entity={} died on terrain, movement stops", unit_id));
            break;
        }
    }
    ctx_.publish_event(
        CombatEvent{.type = EventType::MovementCompleted, .phase = EventPhase::AFTER,
                    .source = unit_id},
        "Action", {f_int("entity", unit_id)},
        std::format("entity={} movement done", unit_id));
    res.ok = true;
    return res;
}

AttackResult ActionResolver::try_attack(EntityId attacker_id, EntityId target_id) {
    AttackResult res;
    UnitRuntime* attacker = ctx_.unit(attacker_id);
    const UnitRuntime* target = ctx_.unit(target_id);
    if (!attacker || !target) {
        res.reject = "unknown_unit";
        return res;
    }
    if (ctx_.current_unit() != attacker_id) {
        res.reject = "not_your_turn";
        return res;
    }
    if (!attacker->is_alive()) {
        res.reject = "unit_not_alive";
        return res;
    }
    if (attacker_id == target_id) {
        res.reject = "self_target";
        return res;
    }
    if (attacker->action_points[slot_index(ActionSlot::BASIC_ATTACK)] < 1) {
        res.reject = "no_attack_action";
        return res;
    }
    if (ctx_.is_ally(attacker_id, target_id)) {
        res.reject = "ally_target";
        return res;
    }
    // 可攻击倒地单位（摧毁尸体，DD-007）；已死者不可
    if (target->is_gone()) {
        res.reject = "target_gone";
        return res;
    }
    if (!GridMap::adjacent8(attacker->pos, target->pos)) {
        res.reject = "out_of_range";
        return res;
    }

    // ---- 执行 ----
    --attacker->action_points[slot_index(ActionSlot::BASIC_ATTACK)];
    ctx_.publish_event(
        CombatEvent{.type = EventType::AttackDeclared, .phase = EventPhase::BEFORE,
                    .source = attacker_id, .target = target_id},
        "Attack", {}, std::format("{} attacks {}", attacker_id, target_id));

    // 基础攻击伤害（wiki：近战普攻 max(1, STR-5)；Phase 6 起按 basic_attack_id 查定义）
    const int32_t damage = ctx_.stats_.melee_attack_damage(ctx_, attacker_id);
    ctx_.publish_event(
        CombatEvent{.type = EventType::AttackHit, .phase = EventPhase::COMMIT,
                    .source = attacker_id, .target = target_id, .amount = damage},
        "Attack", {}, std::format("{} hits {} for {}", attacker_id, target_id, damage));

    DamageContext dmg;
    dmg.source = attacker_id;
    dmg.target = target_id;
    dmg.type = DamageType::PHYSICAL;
    dmg.base_amount = damage;
    dmg.reason = "basic_attack";
    ctx_.deal_damage(std::move(dmg));

    ctx_.log_.append("Attack", "ActionCompleted",
                     {f_int("entity", attacker_id), f_str("action", "basic_attack")},
                     std::format("{} basic_attack done", attacker_id));
    res.ok = true;
    return res;
}

CastResult ActionResolver::try_cast_ability(EntityId caster_id, uint32_t ability_id,
                                            EntityId target_id, Vec2i target_tile) {
    CastResult res;
    UnitRuntime* caster = ctx_.unit(caster_id);
    if (!caster) {
        res.reject = "unknown_unit";
        return res;
    }
    const AbilityDef* ability = ctx_.catalog().ability_def(ability_id);
    if (!ability) {
        res.reject = "unknown_ability";
        return res;
    }
    if (ctx_.current_unit() != caster_id) {
        res.reject = "not_your_turn";
        return res;
    }
    if (!caster->is_alive()) {
        res.reject = "unit_not_alive";
        return res;
    }
    // 技能必须在单位的技能列表中（数据驱动的内容边界）
    const auto& owned = ctx_.def_of(caster_id).ability_ids;
    if (std::find(owned.begin(), owned.end(), ability_id) == owned.end()) {
        res.reject = "ability_not_owned";
        return res;
    }
    if (ability->spends_action
        && caster->action_points[slot_index(ActionSlot::BASIC_ATTACK)] < 1) {
        res.reject = "no_attack_action";
        return res;
    }
    if (caster->mana < ability->mana_cost) {
        res.reject = "not_enough_mana";
        return res;
    }

    // ---- 目标合法性（按目标策略）----
    const UnitRuntime* target_unit = ctx_.unit(target_id);
    const Vec2i target_pos = target_unit && target_unit->pos.x >= 0 ? target_unit->pos : target_tile;
    switch (ability->target_policy) {
        case AbilityTargetPolicy::EnemyUnit:
            if (!target_unit || !target_unit->is_alive() || ctx_.is_ally(caster_id, target_id)) {
                res.reject = "invalid_target";
                return res;
            }
            break;
        case AbilityTargetPolicy::AllyUnit:
            if (!target_unit || !target_unit->is_alive() || !ctx_.is_ally(caster_id, target_id)) {
                res.reject = "invalid_target";
                return res;
            }
            break;
        case AbilityTargetPolicy::AnyUnit:
            if (!target_unit || !target_unit->is_alive()) {
                res.reject = "invalid_target";
                return res;
            }
            break;
        case AbilityTargetPolicy::EmptyTile:
            if (!ctx_.grid().in_bounds(target_tile) || !ctx_.grid().terrain_passable(target_tile)) {
                res.reject = "invalid_target";
                return res;
            }
            break;
        case AbilityTargetPolicy::Self:
            target_id = caster_id;
            break;
    }

    // ---- 距离与视线 ----
    const int32_t dist = GridMap::manhattan(caster->pos, target_pos);
    if (dist < ability->min_range || dist > ability->max_range) {
        res.reject = "out_of_range";
        return res;
    }
    if (ability->needs_los && !ctx_.grid().has_line_of_sight(caster->pos, target_pos)) {
        res.reject = "no_line_of_sight";
        return res;
    }

    // ---- 支付成本（设计文档 §7.1 第 4 步：先校验后支付）----
    if (ability->mana_cost > 0) {
        if (!ctx_.spend_resource(caster_id, ResourceType::MANA, ability->mana_cost, "ability_cast")) {
            res.reject = "not_enough_mana";
            return res;
        }
    }
    if (ability->spends_action) {
        --caster->action_points[slot_index(ActionSlot::BASIC_ATTACK)];
    }

    // ---- 执行（§7.1 第 3~7 步：前置事件 → payload 按定义序 → 后置事件）----
    ctx_.publish_event(
        CombatEvent{.type = EventType::AbilityCastStarted, .phase = EventPhase::BEFORE,
                    .source = caster_id, .target = target_id, .ability_id = ability_id,
                    .position = target_pos},
        "Ability",
        {f_str("name", ability->display_name)},
        std::format("{} casts {} at ({},{})", caster_id, ability->display_name, target_pos.x,
                    target_pos.y));

    // 合成一次施放上下文事件（payload 效果的目标选择器基于它）
    CombatEvent cast_ev;
    cast_ev.type = EventType::AbilityCastStarted;
    cast_ev.phase = EventPhase::COMMIT;
    cast_ev.source = caster_id;
    cast_ev.target = target_id;
    cast_ev.ability_id = ability_id;
    cast_ev.position = target_pos;

    TargetSelector selector(ctx_);
    for (const auto& eff : ability->payload) {
        std::vector<EntityId> targets;
        if (eff.target == TargetSelectorType::CHOSEN_TARGET) {
            targets = {target_id};
        } else if (eff.target == TargetSelectorType::UNITS_ON_TILES) {
            // 溅射（radius）：以目标格为中心收集范围内单位
            CombatEvent radius_ev = cast_ev;
            radius_ev.amount = eff.radius;
            TargetContext tc{radius_ev, caster_id};
            targets = selector.resolve(eff, tc);
        } else {
            TargetContext tc{cast_ev, caster_id};
            targets = selector.resolve(eff, tc);
        }
        if (targets.empty()) {
            ctx_.log_.trace("Ability", "PayloadNoTargets",
                            std::format("ability={} effect={}", ability_id, eff.id));
            continue;
        }
        ctx_.effects().execute(eff.type, ctx_, eff, cast_ev, caster_id, targets);
    }

    ctx_.publish_event(
        CombatEvent{.type = EventType::AbilityCastCompleted, .phase = EventPhase::AFTER,
                    .source = caster_id, .target = target_id, .ability_id = ability_id,
                    .position = target_pos},
        "Ability", {},
        std::format("{} finished casting {}", caster_id, ability->display_name));
    res.ok = true;
    return res;
}

}  // namespace mew::combat
