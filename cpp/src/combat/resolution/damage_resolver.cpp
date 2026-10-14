#include "pch.h"
#include "combat/resolution/damage_resolver.h"

#include "combat/core/battle_context.h"

namespace mew::combat {

DamageResult DamageResolver::deal_damage(DamageContext dmg) {
    DamageResult r;
    r.base_amount = std::max(0, dmg.base_amount);
    UnitRuntime* target = ctx_.unit(dmg.target);
    if (!target) {
        ctx_.log_.trace("Damage", "DamageRejected", std::format("unknown target {}", dmg.target));
        return r;
    }

    // ---- 倒地目标：尸体韧性 −1/次（wiki：与伤害数值无关）----
    if (target->is_down()) {
        ctx_.set_last_effective_damage_source(dmg.source);  // 尸体摧毁者也计入击杀归属
        ctx_.publish_event(
            CombatEvent{.type = EventType::HealthDamaged, .phase = EventPhase::COMMIT,
                        .source = dmg.source, .target = dmg.target, .amount = 1},
            "Damage", {},
            std::format("corpse of {} hit by {}", dmg.target, dmg.source));
        ctx_.death_resolver_.damage_corpse(dmg.target, dmg.source);
        r.target_downed = true;
        return r;
    }
    if (!target->is_alive()) {
        ctx_.log_.trace("Damage", "DamageIgnored",
                        std::format("target {} not alive (state={})", dmg.target,
                                    to_string(target->life_state)));
        return r;
    }

    // ---- 1. 减伤（Phase 4：Brace/Petrify 等状态在此接入；当前为 0）----
    r.mitigated_amount = r.base_amount;

    // ---- 2. 背刺 +25%（仅物理；检测在 Phase 6 接朝向，当前恒不触发）----
    r.after_backstab = r.mitigated_amount;
    r.backstab = false;

    // ---- 3. 暴击 ×2（DD-005/006：仅物理、来源可暴击；减伤之后、护盾之前）----
    r.final_amount = r.after_backstab;
    if (dmg.type == DamageType::PHYSICAL && dmg.source != kInvalidEntity) {
        const UnitDef& src_def = ctx_.def_of(dmg.source);
        if (src_def.can_crit) {
            const int32_t chance_bp = ctx_.stats_.crit_chance_bp(ctx_, dmg.source);
            r.crit = ctx_.rng_.roll_chance(chance_bp, "crit_roll");
            if (r.crit) r.final_amount = r.after_backstab * 2;
        }
    }

    // ---- 4. 护盾（Phase 4：Holy Shield 每实例 1 次 → Shield 吸收）----
    r.shield_absorbed = 0;

    // ---- 5. 扣血 ----
    const int32_t hp_before = target->hp;
    const int32_t hp_after = std::max(0, hp_before - r.final_amount);
    target->hp = hp_after;
    r.hp_lost = hp_before - hp_after;
    if (hp_after <= 0) r.target_downed = true;

    if (r.hp_lost > 0) {
        ctx_.set_last_effective_damage_source(dmg.source);  // 击杀归属/反伤判定
    }
    ctx_.log_.append("Damage", "DamageCalculated",
                     {f_int("base", r.base_amount), f_int("mitigated", r.mitigated_amount),
                      f_bool("backstab", r.backstab), f_bool("crit", r.crit),
                      f_int("final", r.final_amount)},
                     std::format("{} -> {} base={} crit={} final={}", dmg.source, dmg.target,
                                 r.base_amount, r.crit ? "yes" : "no", r.final_amount));
    ctx_.publish_event(
        CombatEvent{.type = EventType::DamageApplied, .phase = EventPhase::COMMIT,
                    .source = dmg.source, .target = dmg.target, .amount = r.final_amount},
        "Damage",
        {f_int("hp_lost", r.hp_lost), f_int("hp_remaining", hp_after),
         f_str("type", std::string(to_string(dmg.type))), f_str("reason", dmg.reason)},
        std::format("{} takes {} {} damage (hp {} -> {})", dmg.target, r.final_amount,
                    to_string(dmg.type), hp_before, hp_after));
    if (r.hp_lost > 0) {
        CombatEvent health_ev;
        health_ev.type = EventType::HealthDamaged;
        health_ev.phase = EventPhase::COMMIT;
        health_ev.source = dmg.source;
        health_ev.target = dmg.target;
        health_ev.amount = r.hp_lost;
        health_ev.tags = dmg.event_tags;
        ctx_.publish_event(std::move(health_ev), "Damage", {},
                           std::format("{} loses {} hp", dmg.target, r.hp_lost));
    }

    return r;
}

}  // namespace mew::combat
