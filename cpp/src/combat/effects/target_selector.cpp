#include "pch.h"
#include "combat/effects/target_selector.h"

#include "combat/core/battle_context.h"
#include "combat/domain/combat_event.h"

namespace mew::combat {

std::vector<EntityId> TargetSelector::units_of_team(Team team) const {
    std::vector<EntityId> out;
    for (const auto& u : ctx_.units()) {
        if (u.team == team && u.is_alive()) out.push_back(u.entity_id);
    }
    return out;  // units_ 本身按 entity_id 升序（= 出生序）
}

std::vector<EntityId> TargetSelector::all_alive_units() const {
    std::vector<EntityId> out;
    for (const auto& u : ctx_.units()) {
        if (u.is_alive()) out.push_back(u.entity_id);
    }
    return out;
}

std::vector<EntityId> TargetSelector::resolve(const EffectDef& def, const TargetContext& tc) const {
    const UnitRuntime* owner = ctx_.unit(tc.owner);
    switch (def.target) {
        case TargetSelectorType::SELF:
            return owner ? std::vector<EntityId>{tc.owner} : std::vector<EntityId>{};

        case TargetSelectorType::EVENT_SOURCE:
            if (tc.event.source != kInvalidEntity && ctx_.unit(tc.event.source))
                return {tc.event.source};
            return {};

        case TargetSelectorType::EVENT_TARGET:
            if (tc.event.target != kInvalidEntity && ctx_.unit(tc.event.target))
                return {tc.event.target};
            return {};

        case TargetSelectorType::CHOSEN_TARGET:
            // Phase 6（行动选定目标）接入；当前无行动上下文 → 空
            return {};

        case TargetSelectorType::ALL_ALLIES:
            return owner ? units_of_team(owner->team) : std::vector<EntityId>{};

        case TargetSelectorType::ALL_ENEMIES: {
            if (!owner) return {};
            Team enemy_team = owner->team == Team::PLAYER ? Team::ENEMY
                              : owner->team == Team::ENEMY ? Team::PLAYER
                                                           : Team::NEUTRAL;
            return units_of_team(enemy_team);
        }

        case TargetSelectorType::ALL_UNITS:
            return all_alive_units();

        case TargetSelectorType::NEAREST_ENEMY: {
            if (!owner) return {};
            EntityId best = kInvalidEntity;
            int32_t best_dist = std::numeric_limits<int32_t>::max();
            for (const auto& u : ctx_.units()) {
                if (u.team == owner->team || !u.is_alive()) continue;
                const int32_t d = GridMap::manhattan(owner->pos, u.pos);
                if (d < best_dist || (d == best_dist && u.entity_id < best)) {
                    best_dist = d;
                    best = u.entity_id;
                }
            }
            return best == kInvalidEntity ? std::vector<EntityId>{} : std::vector<EntityId>{best};
        }

        case TargetSelectorType::RANDOM_ENEMY: {
            if (!owner) return {};
            std::vector<EntityId> enemies;
            for (const auto& u : ctx_.units()) {
                if (u.team != owner->team && u.is_alive()) enemies.push_back(u.entity_id);
            }
            if (enemies.empty()) return {};
            ctx_.log().trace("Target", "RandomTargetCandidates",
                             std::format("owner={} candidates={}", tc.owner, enemies.size()));
            const uint32_t pick = ctx_.rng().next_u32(static_cast<uint32_t>(enemies.size()),
                                                      "random_enemy_target");
            return {enemies[pick]};
        }

        case TargetSelectorType::LOWEST_HP_ALLY: {
            if (!owner) return {};
            EntityId best = kInvalidEntity;
            int32_t best_hp = std::numeric_limits<int32_t>::max();
            for (const auto& u : ctx_.units()) {
                if (u.team != owner->team || !u.is_alive()) continue;
                if (u.hp < best_hp || (u.hp == best_hp && u.entity_id < best)) {
                    best_hp = u.hp;
                    best = u.entity_id;
                }
            }
            return best == kInvalidEntity ? std::vector<EntityId>{} : std::vector<EntityId>{best};
        }

        case TargetSelectorType::HIGHEST_STAT_UNIT: {
            EntityId best = kInvalidEntity;
            int32_t best_val = std::numeric_limits<int32_t>::min();
            for (const auto& u : ctx_.units()) {
                if (!u.is_alive()) continue;
                const int32_t v = ctx_.stats().effective_stat(ctx_, u.entity_id, def.stat);
                if (v > best_val || (v == best_val && u.entity_id < best)) {
                    best_val = v;
                    best = u.entity_id;
                }
            }
            return best == kInvalidEntity ? std::vector<EntityId>{} : std::vector<EntityId>{best};
        }

        case TargetSelectorType::UNITS_IN_RADIUS: {
            const Vec2i center = tc.event.position.x >= 0 ? tc.event.position : (owner ? owner->pos : Vec2i{-1, -1});
            if (center.x < 0) return {};
            std::vector<EntityId> out;
            for (const auto& u : ctx_.units()) {
                if (!u.is_alive()) continue;
                if (GridMap::manhattan(center, u.pos) <= def.radius) out.push_back(u.entity_id);
            }
            return out;
        }

        case TargetSelectorType::UNITS_ON_TILES:
            // Phase 6（指定格子集合）接入
            return {};

        case TargetSelectorType::LAST_EFFECTIVE_DAMAGE_SOURCE: {
            const EntityId src = ctx_.last_effective_damage_source();
            if (src != kInvalidEntity && ctx_.unit(src)) return {src};
            return {};
        }
    }
    return {};
}

}  // namespace mew::combat
