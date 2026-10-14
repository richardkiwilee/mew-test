#include "battle_controller.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include "combat/content/builtin_content.h"
#include "combat/core/battle_context.h"
#include "pch.h"

namespace mew_gdext {

using namespace mew::combat;

namespace {

godot::Dictionary result_ok() {
    godot::Dictionary d;
    d["ok"] = true;
    d["reject"] = "";
    return d;
}

godot::Dictionary result_reject(const std::string& reason) {
    godot::Dictionary d;
    d["ok"] = false;
    d["reject"] = godot::String(reason.c_str());
    return d;
}

}  // namespace

void BattleController::_bind_methods() {
    using namespace godot;
    ClassDB::bind_method(D_METHOD("create_battle", "seed", "w", "h", "terrain"),
                         &BattleController::create_battle);
    ClassDB::bind_method(D_METHOD("add_cat", "stats", "passive_ids", "ability_ids", "x", "y", "name"),
                         &BattleController::add_cat);
    ClassDB::bind_method(D_METHOD("add_enemy", "def_id", "x", "y"), &BattleController::add_enemy);
    ClassDB::bind_method(D_METHOD("start_battle"), &BattleController::start_battle);
    ClassDB::bind_method(D_METHOD("unit_info", "id"), &BattleController::unit_info);
    ClassDB::bind_method(D_METHOD("all_units"), &BattleController::all_units);
    ClassDB::bind_method(D_METHOD("current_unit"), &BattleController::current_unit);
    ClassDB::bind_method(D_METHOD("battle_over"), &BattleController::battle_over);
    ClassDB::bind_method(D_METHOD("winner"), &BattleController::winner);
    ClassDB::bind_method(D_METHOD("round_index"), &BattleController::round_index);
    ClassDB::bind_method(D_METHOD("reachable_tiles", "id"), &BattleController::reachable_tiles);
    ClassDB::bind_method(D_METHOD("path_to", "id", "x", "y"), &BattleController::path_to);
    ClassDB::bind_method(D_METHOD("try_move_to", "id", "x", "y"), &BattleController::try_move_to);
    ClassDB::bind_method(D_METHOD("try_attack", "id", "target_id"), &BattleController::try_attack);
    ClassDB::bind_method(D_METHOD("try_cast", "id", "ability_id", "target_id", "tx", "ty"),
                         &BattleController::try_cast);
    ClassDB::bind_method(D_METHOD("end_turn"), &BattleController::end_turn);
    ClassDB::bind_method(D_METHOD("run_enemy_turn"), &BattleController::run_enemy_turn);
    ClassDB::bind_method(D_METHOD("log_entries"), &BattleController::log_entries);
    ClassDB::bind_method(D_METHOD("log_text"), &BattleController::log_text);
    ClassDB::bind_method(D_METHOD("ability_pool"), &BattleController::ability_pool);
    ClassDB::bind_method(D_METHOD("passive_pool"), &BattleController::passive_pool);
    ClassDB::bind_method(D_METHOD("enemy_pool"), &BattleController::enemy_pool);
}

BattleController::BattleController() = default;
BattleController::~BattleController() = default;

// ---------------------------------------------------------------------------
// 准备阶段
// ---------------------------------------------------------------------------
void BattleController::create_battle(int64_t seed, int64_t w, int64_t h,
                                     const godot::PackedInt32Array& terrain) {
    battle_ = std::make_unique<BattleContext>(static_cast<uint64_t>(seed));
    std::vector<TerrainType> tiles(static_cast<size_t>(w) * h, TerrainType::EMPTY);
    for (int64_t i = 0; i < terrain.size() && i < static_cast<int64_t>(tiles.size()); ++i) {
        const int32_t code = terrain[static_cast<int>(i)];
        tiles[static_cast<size_t>(i)] = code >= 1 && code <= 14
                                            ? static_cast<TerrainType>(code)
                                            : TerrainType::EMPTY;
    }
    battle_->set_grid(static_cast<int32_t>(w), static_cast<int32_t>(h), tiles);
    register_builtin_content(battle_->catalog());
}

int64_t BattleController::add_cat(const godot::PackedInt32Array& stats,
                                  const godot::PackedInt32Array& passive_ids,
                                  const godot::PackedInt32Array& ability_ids, int64_t x,
                                  int64_t y, const godot::String& name) {
    if (!battle_) return -1;
    std::array<int32_t, kStatCount> s{};
    for (int i = 0; i < stats.size() && i < static_cast<int>(kStatCount); ++i) {
        s[static_cast<size_t>(i)] = stats[i];
    }
    std::vector<uint32_t> passives;
    for (int i = 0; i < passive_ids.size(); ++i) passives.push_back(static_cast<uint32_t>(passive_ids[i]));
    UnitDef def = make_cat_def(static_cast<uint32_t>(next_cat_def_id_++), s,
                               std::string(name.utf8().get_data()));
    def.passive_ids = passives;
    for (int i = 0; i < ability_ids.size(); ++i)
        def.ability_ids.push_back(static_cast<uint32_t>(ability_ids[i]));
    battle_->catalog().register_unit_def(def);
    return static_cast<int64_t>(battle_->spawn_unit(def, Team::PLAYER,
                                                    {static_cast<int32_t>(x), static_cast<int32_t>(y)}));
}

int64_t BattleController::add_enemy(int64_t def_id, int64_t x, int64_t y) {
    if (!battle_) return -1;
    const UnitDef* def = battle_->catalog().unit_def(static_cast<uint32_t>(def_id));
    if (!def) return -1;
    return static_cast<int64_t>(battle_->spawn_unit(
        *def, def->default_team, {static_cast<int32_t>(x), static_cast<int32_t>(y)}));
}

int64_t BattleController::start_battle() {
    if (!battle_) return -1;
    return static_cast<int64_t>(battle_->start_battle());
}

// ---------------------------------------------------------------------------
// 状态查询
// ---------------------------------------------------------------------------
godot::Dictionary BattleController::unit_info(int64_t id) const {
    godot::Dictionary d;
    if (!battle_) return d;
    const EntityId eid = static_cast<EntityId>(id);
    const UnitRuntime* u = battle_->unit(eid);
    if (!u) return d;
    const UnitDef& def = battle_->def_of(eid);

    d["id"] = static_cast<int64_t>(eid);
    d["def"] = static_cast<int64_t>(def.id);
    d["name"] = godot::String(def.display_name.c_str());
    d["team"] = static_cast<int64_t>(u->team);
    d["on_board"] = u->pos.x >= 0 && !u->is_gone();
    d["x"] = u->pos.x;
    d["y"] = u->pos.y;
    d["hp"] = u->hp;
    d["max_hp"] = battle_->stats().max_hp(*battle_, eid);
    d["mana"] = u->mana;
    d["max_mana"] = battle_->stats().max_mana(*battle_, eid);
    d["alive"] = u->is_alive();
    d["downed"] = u->is_down();
    d["corpse"] = u->corpse_health_remaining;

    godot::Dictionary stats;
    for (uint32_t i = 1; i <= kStatCount; ++i) {
        const StatType st = static_cast<StatType>(i);
        std::string key(mew::combat::to_string(st));
        for (char& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        stats[godot::String(key.c_str())] = battle_->stats().effective_stat(*battle_, eid, st);
    }
    d["stats"] = stats;

    godot::Array statuses;
    for (const auto& inst : battle_->status_system().all()) {
        if (inst.target_entity != eid) continue;
        const StatusDef* sdef = battle_->catalog().status_def(inst.status_def_id);
        godot::Dictionary sd;
        sd["id"] = static_cast<int64_t>(inst.status_def_id);
        sd["name"] = godot::String(sdef ? sdef->display_name.c_str() : "?");
        sd["stacks"] = inst.stacks;
        sd["duration"] = inst.remaining_duration;
        statuses.append(sd);
    }
    d["statuses"] = statuses;

    godot::Array points;
    for (int32_t p : u->action_points) points.append(p);
    d["action_points"] = points;

    godot::Array abilities;
    for (uint32_t aid : def.ability_ids) {
        const AbilityDef* adef = battle_->catalog().ability_def(aid);
        if (adef) abilities.append(static_cast<int64_t>(aid));
    }
    d["abilities"] = abilities;
    return d;
}

godot::Array BattleController::all_units() const {
    godot::Array out;
    if (!battle_) return out;
    for (const auto& u : battle_->units()) {
        out.append(unit_info(static_cast<int64_t>(u.entity_id)));
    }
    return out;
}

int64_t BattleController::current_unit() const {
    return battle_ ? static_cast<int64_t>(battle_->current_unit()) : 0;
}

bool BattleController::battle_over() const { return battle_ && battle_->battle_over(); }
int64_t BattleController::winner() const {
    return battle_ ? static_cast<int64_t>(battle_->winner()) : 0;
}
int64_t BattleController::round_index() const {
    return battle_ ? static_cast<int64_t>(battle_->scheduler().round_index()) : 0;
}

// ---------------------------------------------------------------------------
// 行动意图
// ---------------------------------------------------------------------------
godot::PackedVector2Array BattleController::reachable_tiles(int64_t id) const {
    godot::PackedVector2Array out;
    if (!battle_) return out;
    for (Vec2i p : battle_->reachable_tiles(static_cast<EntityId>(id), false)) {
        out.append(godot::Vector2i(p.x, p.y));
    }
    return out;
}

godot::PackedVector2Array BattleController::path_to(int64_t id, int64_t x, int64_t y) const {
    godot::PackedVector2Array out;
    if (!battle_) return out;
    for (Vec2i p : battle_->path_to(static_cast<EntityId>(id),
                                    {static_cast<int32_t>(x), static_cast<int32_t>(y)})) {
        out.append(godot::Vector2i(p.x, p.y));
    }
    return out;
}

godot::Dictionary BattleController::try_move_to(int64_t id, int64_t x, int64_t y) {
    if (!battle_) return result_reject("no_battle");
    const EntityId eid = static_cast<EntityId>(id);
    const UnitRuntime* u = battle_->unit(eid);
    if (!u) return result_reject("unknown_unit");
    const auto path = battle_->path_to(eid, {static_cast<int32_t>(x), static_cast<int32_t>(y)});
    if (path.empty()) return result_reject("no_path");
    const MoveResult res = battle_->try_move(eid, path);
    return res.ok ? result_ok() : result_reject(res.reject);
}

godot::Dictionary BattleController::try_attack(int64_t id, int64_t target_id) {
    if (!battle_) return result_reject("no_battle");
    const AttackResult res = battle_->try_attack(static_cast<EntityId>(id),
                                                 static_cast<EntityId>(target_id));
    return res.ok ? result_ok() : result_reject(res.reject);
}

godot::Dictionary BattleController::try_cast(int64_t id, int64_t ability_id, int64_t target_id,
                                             int64_t tx, int64_t ty) {
    if (!battle_) return result_reject("no_battle");
    const CastResult res = battle_->try_cast_ability(
        static_cast<EntityId>(id), static_cast<uint32_t>(ability_id),
        static_cast<EntityId>(target_id), {static_cast<int32_t>(tx), static_cast<int32_t>(ty)});
    return res.ok ? result_ok() : result_reject(res.reject);
}

void BattleController::end_turn() {
    if (!battle_) return;
    battle_->end_current_turn();
    battle_->begin_next_turn();  // 自动推进到下一个单位回合
}

bool BattleController::run_enemy_turn() {
    return battle_ ? battle_->run_ai_turn() : false;
}

// ---------------------------------------------------------------------------
// 战斗日志
// ---------------------------------------------------------------------------
godot::Array BattleController::log_entries() const {
    godot::Array out;
    if (!battle_) return out;
    for (const auto& e : battle_->log().entries()) {
        godot::Dictionary d;
        d["seq"] = static_cast<int64_t>(e.sequence);
        d["round"] = static_cast<int64_t>(e.round_index);
        d["actor"] = static_cast<int64_t>(e.current_actor);
        d["category"] = godot::String(e.category.c_str());
        d["key"] = godot::String(e.message_key.c_str());
        d["detail"] = godot::String(CombatLog::render_entry(e).c_str());
        out.append(d);
    }
    return out;
}

godot::String BattleController::log_text() const {
    return battle_ ? godot::String(battle_->log().to_text().c_str()) : godot::String();
}

// ---------------------------------------------------------------------------
// 内容查询
// ---------------------------------------------------------------------------
godot::Array BattleController::ability_pool() const {
    godot::Array out;
    if (!battle_) return out;
    for (const auto& [id, def] : battle_->catalog().unit_defs()) {
        (void)id;
        (void)def;
    }
    // 从内置常量注册一个临时目录拿技能列表
    DefinitionCatalog tmp;
    register_builtin_content(tmp);
    const auto& unit_ids_list = {unit_ids::kCat};
    (void)unit_ids_list;
    // 技能定义不在 catalog 的公开迭代器里：直接列出内置技能 ID
    const uint32_t ids[] = {ability_ids::kFireball, ability_ids::kHeal, ability_ids::kZap,
                            ability_ids::kFlameSweep, ability_ids::kPoisonSpit};
    for (uint32_t aid : ids) {
        const AbilityDef* def = tmp.ability_def(aid);
        if (!def) continue;
        godot::Dictionary d;
        d["id"] = static_cast<int64_t>(aid);
        d["name"] = godot::String(def->display_name.c_str());
        d["desc"] = godot::String(def->description.c_str());
        d["cost"] = def->mana_cost;
        d["min_range"] = def->min_range;
        d["max_range"] = def->max_range;
        d["radius"] = def->radius;
        switch (def->target_policy) {
            case AbilityTargetPolicy::EnemyUnit: d["target"] = "enemy"; break;
            case AbilityTargetPolicy::AllyUnit:  d["target"] = "ally"; break;
            case AbilityTargetPolicy::AnyUnit:   d["target"] = "any"; break;
            case AbilityTargetPolicy::EmptyTile: d["target"] = "tile"; break;
            case AbilityTargetPolicy::Self:      d["target"] = "self"; break;
        }
        out.append(d);
    }
    return out;
}

godot::Array BattleController::passive_pool() const {
    godot::Array out;
    DefinitionCatalog tmp;
    register_builtin_content(tmp);
    const uint32_t ids[] = {passive_ids::kSuperLuck, passive_ids::kSwapExtremes,
                            passive_ids::kBrave, passive_ids::kAgile,
                            passive_ids::kManaScholar, passive_ids::kHunterReward,
                            passive_ids::kThorns, passive_ids::kRegen};
    for (uint32_t pid : ids) {
        const PassiveDef* def = tmp.passive_def(pid);
        if (!def) continue;
        godot::Dictionary d;
        d["id"] = static_cast<int64_t>(pid);
        d["name"] = godot::String(def->display_name.c_str());
        d["desc"] = godot::String(def->description.c_str());
        out.append(d);
    }
    return out;
}

godot::Array BattleController::enemy_pool() const {
    godot::Array out;
    DefinitionCatalog tmp;
    register_builtin_content(tmp);
    const uint32_t ids[] = {unit_ids::kRat, unit_ids::kBat, unit_ids::kThug, unit_ids::kImp,
                            unit_ids::kSpitter};
    for (uint32_t uid : ids) {
        const UnitDef* def = tmp.unit_def(uid);
        if (!def) continue;
        godot::Dictionary d;
        d["id"] = static_cast<int64_t>(uid);
        d["name"] = godot::String(def->display_name.c_str());
        godot::Array stats;
        for (int32_t s : def->base_stats) stats.append(s);
        d["stats"] = stats;
        out.append(d);
    }
    return out;
}

}  // namespace mew_gdext
