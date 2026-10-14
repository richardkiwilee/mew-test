#include "pch.h"
#include "combat/resolution/death_resolver.h"

#include "combat/core/battle_context.h"

namespace mew::combat {

void DeathResolver::process_deaths(EntityId last_damage_source) {
    // 统一死亡处理（DD-007）：
    //  ALIVE 且 hp<=0：
    //    有尸体韧性（玩家猫）→ DOWNED（留在场上，可被攻击/复活）
    //    无尸体韧性（敌人）  → DEAD（移除）
    for (auto& u : ctx_.units_) {
        if (u.life_state != LifeState::ALIVE || u.hp > 0) continue;
        const UnitDef& def = ctx_.def_of(u.entity_id);
        if (def.corpse_health > 0) {
            u.life_state = LifeState::DOWNED;
            ctx_.status_system_.clear_on_down(u.entity_id);  // DD-007：倒地清除状态
            ctx_.publish_event(
                CombatEvent{.type = EventType::UnitDowned, .phase = EventPhase::COMMIT,
                            .source = last_damage_source, .target = u.entity_id},
                "Death",
                {f_int("entity", u.entity_id), f_int("killer", last_damage_source)},
                std::format("entity={} downed (killer={})", u.entity_id, last_damage_source));
        } else {
            u.life_state = LifeState::DEAD;
            if (u.pos.x >= 0) ctx_.grid_.clear_occupant(u.pos);
            u.pos = {-1, -1};
            ctx_.publish_event(
                CombatEvent{.type = EventType::UnitDied, .phase = EventPhase::COMMIT,
                            .source = last_damage_source, .target = u.entity_id},
                "Death",
                {f_int("entity", u.entity_id), f_int("killer", last_damage_source)},
                std::format("entity={} died (killer={})", u.entity_id, last_damage_source));
        }
    }
    check_battle_end();
}

void DeathResolver::damage_corpse(EntityId id, EntityId attacker) {
    UnitRuntime* u = ctx_.unit(id);
    if (!u || !u->is_down()) return;
    --u->corpse_health_remaining;
    ctx_.log_.append("Death", "CorpseHealthLost",
                     {f_int("entity", id), f_int("remaining", u->corpse_health_remaining)},
                     std::format("corpse of {} -1 ({} left)", id, u->corpse_health_remaining));
    ctx_.publish_event(
        CombatEvent{.type = EventType::HealthDamaged, .phase = EventPhase::COMMIT,
                    .source = attacker, .target = id, .amount = 1},
        "Death", {}, std::format("corpse of {} damaged by {}", id, attacker));
    if (u->corpse_health_remaining <= 0) {
        u->life_state = LifeState::DEAD;
        if (u->pos.x >= 0) ctx_.grid_.clear_occupant(u->pos);
        u->pos = {-1, -1};
        ctx_.publish_event(
            CombatEvent{.type = EventType::UnitDied, .phase = EventPhase::COMMIT,
                        .source = attacker, .target = id},
            "Death",
            {f_int("entity", id), f_int("killer", attacker)},
            std::format("entity={} corpse destroyed by {}", id, attacker));
        check_battle_end();
    }
}

void DeathResolver::check_battle_end() {
    if (ctx_.battle_over_) return;

    // 胜利：所有"必需击杀"的敌人死亡/移除
    bool required_enemy_remains = false;
    for (const auto& u : ctx_.units_) {
        if (u.team != Team::ENEMY) continue;
        if (!ctx_.def_of(u.entity_id).is_required_enemy) continue;
        if (!u.is_gone()) {
            required_enemy_remains = true;
            break;
        }
    }
    if (!required_enemy_remains && !ctx_.units_.empty()) {
        ctx_.battle_over_ = true;
        ctx_.winner_ = Team::PLAYER;
        ctx_.log_.append("Battle", "BattleEnded",
                         {f_str("result", "victory"), f_int("round", ctx_.scheduler_.round_index())},
                         "victory: all required enemies defeated");
        ctx_.apply_victory_cleanup();
        ctx_.scheduler_.stop();
        return;
    }

    // 失败：所有玩家猫倒地/死亡
    bool player_can_fight = false;
    for (const auto& u : ctx_.units_) {
        if (u.team == Team::PLAYER && u.is_alive()) {
            player_can_fight = true;
            break;
        }
    }
    if (!player_can_fight && !ctx_.units_.empty()) {
        ctx_.battle_over_ = true;
        ctx_.winner_ = Team::ENEMY;
        ctx_.log_.append("Battle", "BattleEnded",
                         {f_str("result", "defeat"), f_int("round", ctx_.scheduler_.round_index())},
                         "defeat: all player cats downed");
        ctx_.scheduler_.stop();
        return;
    }
}

}  // namespace mew::combat
