#include "pch.h"
#include "combat/effects/effect_registry.h"

#include "combat/core/battle_context.h"
#include "combat/domain/combat_event.h"

namespace mew::combat {

void EffectRegistry::register_handler(EffectType type, EffectHandler handler) {
    handlers_[type] = std::move(handler);
}

bool EffectRegistry::execute(EffectType type, BattleContext& ctx, const EffectDef& def,
                             const CombatEvent& event, EntityId owner,
                             const std::vector<EntityId>& targets) {
    const auto it = handlers_.find(type);
    if (it == handlers_.end()) {
        ctx.log().trace("Effect", "EffectUnregistered",
                        std::format("type={} (no handler)", to_string(type)));
        return false;
    }
    it->second(ctx, def, event, owner, targets);
    return true;
}

namespace {

// ---- 内置原子效果 ----

void h_apply_status(BattleContext& ctx, const EffectDef& def, const CombatEvent&, EntityId owner,
                    const std::vector<EntityId>& targets) {
    for (EntityId t : targets) {
        ctx.status_system().apply(t, def.status_id, def.stacks, owner, "effect");
    }
}

void h_remove_status(BattleContext& ctx, const EffectDef& def, const CombatEvent&, EntityId,
                     const std::vector<EntityId>& targets) {
    for (EntityId t : targets) {
        ctx.status_system().remove(t, def.status_id, "effect");
    }
}

void h_modify_stacks(BattleContext& ctx, const EffectDef& def, const CombatEvent&, EntityId owner,
                     const std::vector<EntityId>& targets) {
    for (EntityId t : targets) {
        // 简化语义：正数 = 施加堆叠（走统一 apply），负数 = 按定义移除
        if (def.amount > 0) {
            ctx.status_system().apply(t, def.status_id, static_cast<int32_t>(def.amount), owner,
                                      "effect");
        } else {
            ctx.status_system().remove(t, def.status_id, "effect");
        }
    }
}

void h_gain_resource(BattleContext& ctx, const EffectDef& def, const CombatEvent&, EntityId,
                     const std::vector<EntityId>& targets) {
    for (EntityId t : targets) {
        ctx.gain_resource(t, def.resource, static_cast<int32_t>(def.amount));
    }
}

void h_spend_resource(BattleContext& ctx, const EffectDef& def, const CombatEvent&, EntityId,
                      const std::vector<EntityId>& targets) {
    for (EntityId t : targets) {
        ctx.spend_resource(t, def.resource, static_cast<int32_t>(def.amount), "effect");
    }
}

void h_heal(BattleContext& ctx, const EffectDef& def, const CombatEvent&, EntityId,
            const std::vector<EntityId>& targets) {
    for (EntityId t : targets) {
        ctx.heal(t, static_cast<int32_t>(def.amount), "effect");
    }
}

void h_deal_damage(BattleContext& ctx, const EffectDef& def, const CombatEvent& event, EntityId,
                   const std::vector<EntityId>& targets) {
    for (EntityId t : targets) {
        DamageContext dmg;
        dmg.source = event.source;  // 状态施加者或环境（kInvalidEntity）
        dmg.target = t;
        dmg.type = def.damage_type;
        dmg.base_amount = static_cast<int32_t>(def.amount);
        dmg.reason = "effect";
        ctx.deal_damage(dmg);
    }
}

void h_grant_extra_turn(BattleContext& ctx, const EffectDef&, const CombatEvent&, EntityId,
                        const std::vector<EntityId>& targets) {
    for (EntityId t : targets) {
        ctx.scheduler().grant_extra_turn(t, "effect");
    }
}

void h_add_tag(BattleContext& ctx, const EffectDef& def, const CombatEvent&, EntityId,
               const std::vector<EntityId>& targets) {
    for (EntityId t : targets) {
        if (UnitRuntime* u = ctx.unit(t)) {
            if (std::find(u->tags.begin(), u->tags.end(), def.text_param) == u->tags.end()) {
                u->tags.push_back(def.text_param);
                ctx.log().append("Effect", "TagAdded",
                                 {f_int("entity", t), f_str("tag", def.text_param)},
                                 std::format("entity={} tag={}", t, def.text_param));
            }
        }
    }
}

void h_remove_tag(BattleContext& ctx, const EffectDef& def, const CombatEvent&, EntityId,
                  const std::vector<EntityId>& targets) {
    for (EntityId t : targets) {
        if (UnitRuntime* u = ctx.unit(t)) {
            const size_t before = u->tags.size();
            std::erase(u->tags, def.text_param);
            if (u->tags.size() != before) {
                ctx.log().append("Effect", "TagRemoved",
                                 {f_int("entity", t), f_str("tag", def.text_param)},
                                 std::format("entity={} tag={}", t, def.text_param));
            }
        }
    }
}

// 召唤：在事件相关位置附近找空格生成单位（确定性环形搜索，设计文档 §12.3）
void h_spawn_unit(BattleContext& ctx, const EffectDef& def, const CombatEvent& event, EntityId owner,
                  const std::vector<EntityId>&) {
    const UnitDef* spawn_def = ctx.catalog().unit_def(def.unit_def_id);
    if (!spawn_def) {
        ctx.log().trace("Effect", "SpawnRejected",
                        std::format("unknown unit def {}", def.unit_def_id));
        return;
    }
    // 定位锚点：事件目标位置 → 事件位置 → 所有者位置
    Vec2i anchor{-1, -1};
    const UnitRuntime* target_u = ctx.unit(event.target);
    if (target_u && target_u->pos.x >= 0) anchor = target_u->pos;
    else if (event.position.x >= 0) anchor = event.position;
    else if (const UnitRuntime* owner_u = ctx.unit(owner)) anchor = owner_u->pos;

    Vec2i spot{-1, -1};
    if (anchor.x >= 0) {
        // 环形确定性搜索（曼哈顿距离 1..3，固定方向序）
        constexpr Vec2i dirs[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        bool found = false;
        for (int32_t r = 1; r <= 3 && !found; ++r) {
            // 以 (r, r) 为界枚举半径 r 环上的候选（先 x 轴后 y 轴，固定顺序）
            for (int32_t dx = -r; dx <= r && !found; ++dx) {
                for (int32_t dy = -r; dy <= r && !found; ++dy) {
                    if (std::abs(dx) + std::abs(dy) != r) continue;
                    const Vec2i p = anchor + Vec2i{dx, dy};
                    if (ctx.grid().enterable(p)) {
                        spot = p;
                        found = true;
                    }
                }
            }
            (void)dirs;
        }
    }
    if (spot.x < 0) {
        ctx.log().append("Effect", "SpawnFailed",
                         {f_int("def", def.unit_def_id), f_int("anchor_x", anchor.x),
                          f_int("anchor_y", anchor.y)},
                         std::format("no free tile near ({},{}) for def {}", anchor.x, anchor.y,
                                     def.unit_def_id));
        return;
    }
    const EntityId spawned = ctx.spawn_unit(*spawn_def, spawn_def->default_team, spot);
    if (spawned != kInvalidEntity) {
        ctx.log().append("Effect", "UnitSummoned",
                         {f_int("entity", spawned), f_int("def", def.unit_def_id),
                          f_int("summoner", owner)},
                         std::format("def {} summoned by {} at ({},{})", def.unit_def_id, owner,
                                     spot.x, spot.y));
    }
}

// 反伤：固定数值反射给事件来源（TRUE 伤害，不引发再次反伤——由伤害类型与来源语义控制）
void h_reflect_damage(BattleContext& ctx, const EffectDef& def, const CombatEvent& event, EntityId,
                      const std::vector<EntityId>& targets) {
    if (event.source == kInvalidEntity) return;  // 环境伤害不反
    for (EntityId t : targets) {
        (void)t;
        if (ctx.unit(event.source) == nullptr || !ctx.unit(event.source)->is_alive()) continue;
        ctx.log().append("Effect", "DamageReflected",
                         {f_int("from", event.target), f_int("to", event.source),
                          f_int("amount", static_cast<int64_t>(def.amount))},
                         std::format("{} reflects {} to {}", event.target, def.amount,
                                     event.source));
        DamageContext dmg;
        dmg.source = event.target;
        dmg.target = event.source;
        dmg.type = DamageType::TRUE;
        dmg.base_amount = static_cast<int32_t>(def.amount);
        dmg.reason = "reflect";
        dmg.event_tags = {"reflect"};  // 反伤伤害不再触发反伤（防无限弹射）
        ctx.deal_damage(dmg);
    }
}

}  // namespace

void EffectRegistry::install_builtin(EffectRegistry& registry) {
    registry.register_handler(EffectType::APPLY_STATUS, h_apply_status);
    registry.register_handler(EffectType::REMOVE_STATUS, h_remove_status);
    registry.register_handler(EffectType::MODIFY_STATUS_STACKS, h_modify_stacks);
    registry.register_handler(EffectType::GAIN_RESOURCE, h_gain_resource);
    registry.register_handler(EffectType::SPEND_RESOURCE, h_spend_resource);
    registry.register_handler(EffectType::HEAL, h_heal);
    registry.register_handler(EffectType::DEAL_DAMAGE, h_deal_damage);
    registry.register_handler(EffectType::GRANT_EXTRA_TURN, h_grant_extra_turn);
    registry.register_handler(EffectType::ADD_TAG, h_add_tag);
    registry.register_handler(EffectType::REMOVE_TAG, h_remove_tag);
    registry.register_handler(EffectType::SPAWN_UNIT, h_spawn_unit);
    registry.register_handler(EffectType::REFLECT_DAMAGE, h_reflect_damage);
}

}  // namespace mew::combat
