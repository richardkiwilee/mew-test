#include "pch.h"
#include "combat/core/battle_context.h"

#include "combat/content/builtin_content.h"

namespace mew::combat {

BattleContext::BattleContext(uint64_t seed)
    : rng_(seed),
      scheduler_(*this),
      damage_resolver_(*this),
      death_resolver_(*this),
      action_resolver_(*this),
      status_system_(*this),
      events_(*this),
      enemy_ai_(*this) {
    EffectRegistry::install_builtin(effect_registry_);
}

void BattleContext::set_grid(int32_t w, int32_t h, const std::vector<TerrainType>& tiles) {
    grid_.resize(w, h);
    for (size_t i = 0; i < tiles.size(); ++i) {
        grid_.tile({static_cast<int32_t>(i % w), static_cast<int32_t>(i / w)}).terrain = tiles[i];
    }
}

EntityId BattleContext::spawn_unit(const UnitDef& def, Team team, Vec2i pos) {
    if (!grid_.in_bounds(pos) || grid_.occupant(pos) != kInvalidEntity || !grid_.terrain_passable(pos)) {
        log_.append("Spawn", "SpawnRejected",
                    {f_int("def", static_cast<int64_t>(def.id)), f_int("x", pos.x), f_int("y", pos.y)},
                    std::format("def={} pos=({},{})", def.id, pos.x, pos.y));
        return kInvalidEntity;
    }
    UnitRuntime u;
    u.entity_id = static_cast<EntityId>(units_.size() + 1);
    u.def_id = def.id;
    u.team = team;
    u.pos = pos;
    u.hp = 0;
    u.mana = 0;
    u.life_state = LifeState::ALIVE;
    u.spawn_seq = static_cast<uint32_t>(units_.size());
    u.corpse_health_remaining = def.corpse_health;
    u.action_points.fill(0);
    grid_.place_unit(u.entity_id, pos);
    const EntityId id = u.entity_id;
    units_.push_back(std::move(u));
    // 初始被动实例化（设计文档 §4.2：共享定义、独立实例）
    for (uint32_t passive_id : def.passive_ids) {
        add_passive(id, passive_id);
    }
    // 属性初始化（烘焙常驻修正；一次性变换在 start_battle 时执行）
    stats_.initialize_stats(*this, id, /*run_transforms=*/false);
    units_.back().hp = stats_.max_hp(*this, id);
    units_.back().mana = stats_.starting_mana(*this, id);
    publish_event(
        CombatEvent{.type = EventType::UnitSpawned, .phase = EventPhase::AFTER, .source = id,
                    .position = pos},
        "Spawn", {f_str("name", def.display_name), f_int("team", static_cast<int64_t>(team)),
                  f_int("hp", units_.back().hp)},
        std::format("entity={} name={} team={} hp={}", id, def.display_name, to_string(team),
                    units_.back().hp));
    return id;
}

EntityId BattleContext::start_battle() {
    assert(!battle_started_ && "battle already started");
    battle_started_ = true;
    // 属性初始化流水线（设计文档 §6.3 流程）：
    // 1) 设定烘焙标记；2) 重建有效基底（含常驻修正）；3) 执行一次性变换
    set_stat_init_markers(passive_seq_, status_system_.instance_seq());
    for (auto& u : units_) {
        stats_.initialize_stats(*this, u.entity_id, /*run_transforms=*/true);
    }
    events_.on_battle_started();
    publish_event(CombatEvent{.type = EventType::BattleStarted, .phase = EventPhase::AFTER},
                  "Battle", {f_int("seed", static_cast<int64_t>(rng_.seed()))},
                  std::format("seed={}", rng_.seed()));
    scheduler_.start_battle();
    return begin_next_turn();
}

EntityId BattleContext::begin_next_turn() {
    if (battle_over_) return kInvalidEntity;
    const EntityId id = scheduler_.begin_next_turn();
    if (id == kInvalidEntity) {
        check_battle_end();
        return kInvalidEntity;
    }
    current_unit_ = id;
    log_.set_current_actor(id);
    log_.set_round(scheduler_.round_index());
    UnitRuntime* u = unit(id);
    ++u->turn_seq;
    events_.on_turn_started();
    publish_event(
        CombatEvent{.type = EventType::TurnStarted, .phase = EventPhase::AFTER, .source = id},
        "Scheduler",
        {f_str("kind", std::string(to_string(scheduler_.last_turn_kind())))},
        std::format("entity={} kind={}", id, to_string(scheduler_.last_turn_kind())));
    refresh_action_points(*u);
    // DD-012：刷新动作后处理回合开始状态 tick（Burn 先伤害再衰减）
    status_system_.tick_start_of_turn(id);
    // 被控制状态阻断（Stun 类，DD-019）：跳过回合（仍推进状态时长，避免永久眩晕）
    if (u->is_alive() && status_system_.blocked_by_status(id)) {
        status_system_.tick_end_of_turn(id);
        publish_event(
            CombatEvent{.type = EventType::TurnEnded, .phase = EventPhase::AFTER, .source = id},
            "Scheduler", {f_bool("blocked", true)},
            std::format("entity={} turn blocked by control status", id));
        log_.append("Scheduler", "TurnSkipped", {f_int("entity", id), f_str("reason", "stunned")},
                    std::format("entity={} stunned", id));
        current_unit_ = kInvalidEntity;
        log_.set_current_actor(kInvalidEntity);
        scheduler_.end_current_turn();
        return begin_next_turn();
    }
    // tick 效果可能杀死单位（回合开始燃烧致死）：取消回合，进入下一个单位
    if (!u->is_alive()) {
        publish_event(
            CombatEvent{.type = EventType::TurnEnded, .phase = EventPhase::AFTER, .source = id},
            "Scheduler", {}, std::format("entity={} died at turn start", id));
        current_unit_ = kInvalidEntity;
        log_.set_current_actor(kInvalidEntity);
        scheduler_.end_current_turn();
        return begin_next_turn();
    }
    return id;
}

void BattleContext::end_current_turn() {
    if (current_unit_ == kInvalidEntity) return;
    UnitRuntime* u = unit(current_unit_);
    if (u) {
        publish_event(
            CombatEvent{.type = EventType::TurnEnding, .phase = EventPhase::AFTER,
                        .source = current_unit_},
            "Scheduler", {}, std::format("entity={}", current_unit_));
        // DD-011：结束前状态 tick → 回复 → 时长推进/过期
        status_system_.tick_end_of_turn(current_unit_);
        if (u->is_alive()) regen_at_turn_end(*u);
    }
    publish_event(
        CombatEvent{.type = EventType::TurnEnded, .phase = EventPhase::AFTER,
                    .source = current_unit_},
        "Scheduler", {}, std::format("entity={}", current_unit_));
    current_unit_ = kInvalidEntity;
    log_.set_current_actor(kInvalidEntity);
    scheduler_.end_current_turn();
}

MoveResult BattleContext::try_move(EntityId unit_id, const std::vector<Vec2i>& path) {
    return action_resolver_.try_move(unit_id, path);
}

AttackResult BattleContext::try_attack(EntityId attacker_id, EntityId target_id) {
    return action_resolver_.try_attack(attacker_id, target_id);
}

CastResult BattleContext::try_cast_ability(EntityId caster_id, uint32_t ability_id,
                                           EntityId target_id, Vec2i target_tile) {
    return action_resolver_.try_cast_ability(caster_id, ability_id, target_id, target_tile);
}

bool BattleContext::run_ai_turn() {
    if (battle_over_) return false;
    if (current_unit_ == kInvalidEntity) return false;
    const UnitRuntime* u = unit(current_unit_);
    if (!u || u->team != Team::ENEMY) return false;
    const bool acted = enemy_ai_.act_for(current_unit_);
    end_current_turn();
    begin_next_turn();
    return acted;
}

void BattleContext::apply_terrain_on_enter(EntityId unit_id, Vec2i pos) {
    UnitRuntime* u = unit(unit_id);
    if (!u || !u->is_alive()) return;
    const TileCell& tile = grid_.tile(pos);
    const int32_t dmg = terrain_enter_damage(tile.terrain);
    const bool wet = terrain_applies_wet(tile.terrain);
    if (dmg > 0) {
        log_.append("Terrain", "TerrainTriggered",
                    {f_int("entity", unit_id), f_str("terrain", std::string(to_string(tile.terrain))),
                     f_int("damage", dmg)},
                    std::format("entity={} enters {} takes {}", unit_id, to_string(tile.terrain),
                                dmg));
        DamageContext d;
        d.source = kInvalidEntity;  // 环境伤害
        d.target = unit_id;
        d.type = DamageType::TRUE;
        d.base_amount = dmg;
        d.reason = "terrain";
        deal_damage(std::move(d));
    }
    if (wet) {
        status_system_.apply(unit_id, status_ids::kWet, 1, kInvalidEntity, "terrain");
    }
}

int32_t BattleContext::terrain_regen_bonus(EntityId unit_id) const {
    const UnitRuntime* u = unit(unit_id);
    if (!u || u->pos.x < 0) return 0;
    const TerrainType t = grid_.tile(u->pos).terrain;
    switch (t) {
        case TerrainType::FLOWER:      return 2;
        case TerrainType::TALL_FLOWER: return 3;
        default:                       return 0;
    }
}

DamageResult BattleContext::deal_damage(DamageContext dmg) {
    const EntityId source = dmg.source;
    DamageResult result = damage_resolver_.deal_damage(std::move(dmg));
    process_deaths(source);
    return result;
}

void BattleContext::process_deaths(EntityId last_damage_source) {
    death_resolver_.process_deaths(last_damage_source);
}

void BattleContext::check_battle_end() {
    death_resolver_.check_battle_end();
}

std::vector<Vec2i> BattleContext::reachable_tiles(EntityId id, bool ignore_occupants) const {
    const UnitRuntime* u = unit(id);
    if (!u) return {};
    return grid_.reachable_tiles(u->pos, move_budget_of(id), ignore_occupants);
}

std::vector<Vec2i> BattleContext::path_to(EntityId id, Vec2i target) const {
    const UnitRuntime* u = unit(id);
    if (!u) return {};
    return grid_.path_to(u->pos, target, move_budget_of(id), /*ignore_occupants=*/false);
}

void BattleContext::refresh_action_points(UnitRuntime& u) {
    // DD-012 / wiki Refresh：动作剩余 0 次 → 补 1 点；有剩余（Bonus 等）不重复补
    for (int32_t& points : u.action_points) {
        if (points <= 0) points = 1;
    }
    log_.append("Scheduler", "ActionsRefreshed",
                {f_int("entity", u.entity_id)},
                std::format("entity={} move={} attack={} weapon={} trinket={}", u.entity_id,
                            u.action_points[static_cast<uint32_t>(ActionSlot::MOVE) - 1],
                            u.action_points[static_cast<uint32_t>(ActionSlot::BASIC_ATTACK) - 1],
                            u.action_points[static_cast<uint32_t>(ActionSlot::WEAPON) - 1],
                            u.action_points[static_cast<uint32_t>(ActionSlot::TRINKET) - 1]));
}

void BattleContext::regen_at_turn_end(UnitRuntime& u) {
    // DD-011：回蓝 = max(INT,0) + 地形加成；回血 = Health Regen + 地形加成（花丛）
    // 全部经统一资源 API（会产生 ResourceGained 事件，供被动订阅）
    const int32_t terrain_bonus = terrain_regen_bonus(u.entity_id);
    const int32_t mana_gain = stats_.mana_regen(*this, u.entity_id) + terrain_bonus;
    const int32_t hp_gain = stats_.health_regen(*this, u.entity_id) + terrain_bonus;
    if (mana_gain > 0) {
        gain_resource(u.entity_id, ResourceType::MANA, mana_gain);
    }
    if (hp_gain > 0) {
        gain_resource(u.entity_id, ResourceType::HP, hp_gain);
    }
    if (terrain_bonus > 0) {
        log_.append("Terrain", "TerrainRegen",
                    {f_int("entity", u.entity_id), f_int("bonus", terrain_bonus)},
                    std::format("entity={} terrain regen +{}", u.entity_id, terrain_bonus));
    }
}

void BattleContext::publish_event(CombatEvent event, std::string_view category,
                                  std::vector<LogField> extra_fields, std::string detail) {
    event.seq = next_event_seq();
    event.event_id = next_event_id();
    std::vector<LogField> fields;
    fields.push_back(f_int("source", event.source));
    fields.push_back(f_int("target", event.target));
    if (event.amount != 0) fields.push_back(f_int("amount", event.amount));
    for (auto& f : extra_fields) fields.push_back(std::move(f));
    log_.append_event(std::string(category), std::string(to_string(event.type)), event.event_id,
                      event.source, event.target, std::move(fields), std::move(detail));
    events_.publish(std::move(event));
}

int32_t BattleContext::gain_resource(EntityId unit_id, ResourceType res, int32_t amount) {
    UnitRuntime* u = unit(unit_id);
    if (!u || !u->is_alive() || amount <= 0) return 0;
    int32_t actual = 0;
    if (res == ResourceType::MANA) {
        actual = std::min(amount, stats_.max_mana(*this, unit_id) - u->mana);
        if (actual > 0) u->mana += actual;
    } else if (res == ResourceType::HP) {
        actual = std::min(amount, stats_.max_hp(*this, unit_id) - u->hp);
        if (actual > 0) u->hp += actual;
    }
    if (actual > 0) {
        publish_event(
            CombatEvent{.type = EventType::ResourceGained, .phase = EventPhase::COMMIT,
                        .source = unit_id, .amount = actual, .resource = res},
            "Resource",
            {},
            std::format("entity={} resource={} amount={}", unit_id, to_string(res), actual));
    }
    return actual;
}

bool BattleContext::spend_resource(EntityId unit_id, ResourceType res, int32_t amount,
                                   std::string_view reason) {
    UnitRuntime* u = unit(unit_id);
    if (!u || amount <= 0) return false;
    int32_t* pool = nullptr;
    if (res == ResourceType::MANA) pool = &u->mana;
    else if (res == ResourceType::HP) pool = &u->hp;
    if (!pool) return false;
    if (*pool < amount) {
        log_.trace("Resource", "ResourceSpendRejected",
                   std::format("entity={} resource={} need={} have={} ({})", unit_id,
                               to_string(res), amount, *pool, reason));
        return false;
    }
    *pool -= amount;
    publish_event(
        CombatEvent{.type = EventType::ResourceSpent, .phase = EventPhase::COMMIT,
                    .source = unit_id, .amount = amount, .resource = res},
        "Resource",
        {f_str("reason", std::string(reason))},
        std::format("entity={} resource={} amount={} ({})", unit_id, to_string(res), amount, reason));
    return true;
}

int32_t BattleContext::heal(EntityId unit_id, int32_t amount, std::string_view reason) {
    UnitRuntime* u = unit(unit_id);
    if (!u || !u->is_alive() || amount <= 0) return 0;  // DD-007：倒地不可治疗
    const int32_t requested = amount;
    const int32_t actual = std::min(amount, stats_.max_hp(*this, unit_id) - u->hp);
    if (actual > 0) {
        u->hp += actual;
        publish_event(
            CombatEvent{.type = EventType::HealingApplied, .phase = EventPhase::COMMIT,
                        .source = unit_id, .amount = actual},
            "Heal",
            {f_int("requested", requested), f_str("reason", std::string(reason))},
            std::format("entity={} heal {} (requested {}) ({})", unit_id, actual, requested, reason));
    }
    return actual;
}

bool BattleContext::revive_unit(EntityId unit_id, int32_t hp_value, std::string_view reason) {
    UnitRuntime* u = unit(unit_id);
    if (!u || !u->is_down()) {
        log_.trace("Death", "ReviveRejected",
                   std::format("entity={} not downed", unit_id));
        return false;
    }
    u->life_state = LifeState::ALIVE;
    u->hp = std::max(1, hp_value);
    u->corpse_health_remaining = def_of(unit_id).corpse_health;
    u->acted_this_round = true;  // 本轮不再行动（下轮入队，DD）
    publish_event(
        CombatEvent{.type = EventType::UnitRevived, .phase = EventPhase::AFTER,
                    .source = unit_id, .amount = u->hp},
        "Death",
        {f_str("reason", std::string(reason))},
        std::format("entity={} revived hp={} ({})", unit_id, u->hp, reason));
    return true;
}

void BattleContext::add_passive(EntityId owner, uint32_t passive_def_id) {
    PassiveRuntime rt;
    rt.instance_id = ++passive_seq_;
    rt.owner_entity = owner;
    rt.passive_def_id = passive_def_id;
    rt.acquisition_seq = static_cast<uint32_t>(passive_runtimes_.size());
    const PassiveDef* def = catalog_.passive_def(passive_def_id);
    if (def) {
        rt.effect_counters.resize(def->effects.size());
    }
    log_.append("Passive", "PassiveGranted",
                {f_int("entity", owner), f_int("passive", passive_def_id),
                 f_str("name", def ? def->display_name : "?")},
                std::format("entity={} passive={}", owner, passive_def_id));
    passive_runtimes_.push_back(std::move(rt));
}

void BattleContext::apply_victory_cleanup() {
    // DD-007：胜利后倒地猫按战后回复值复活
    for (auto& u : units_) {
        if (u.team == Team::PLAYER && u.is_down()) {
            const UnitDef& def = def_of(u.entity_id);
            u.hp = std::min(stats_.post_battle_regen(*this, u.entity_id), stats_.max_hp(*this, u.entity_id));
            u.life_state = LifeState::ALIVE;
            log_.append("Death", "UnitRevived",
                        {f_int("entity", u.entity_id), f_int("hp", u.hp)},
                        std::format("entity={} post_battle_regen hp={}", u.entity_id, u.hp));
        }
    }
}

}  // namespace mew::combat
