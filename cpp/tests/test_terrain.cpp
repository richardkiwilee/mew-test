#include "pch.h"
#include "test_framework.h"

#include "battle_test_utils.h"

using namespace mew::combat;
using namespace mewtest;

namespace {

EntityId cat_with(BattleContext& ctx, const std::array<int32_t, 7>& stats, Vec2i pos,
                  uint32_t def_id) {
    UnitDef def = make_cat_def(def_id, stats, std::format("cat{}", def_id));
    ctx.catalog().register_unit_def(def);
    return ctx.spawn_unit(def, Team::PLAYER, pos);
}

}  // namespace

// 荆棘：每进入一次 2 伤害（wiki 数值）
TEST_CASE(terrain_bramble_damage) {
    BattleContext ctx = make_battle(42);
    std::vector<TerrainType> tiles(10 * 10, TerrainType::EMPTY);
    tiles[2 * 10 + 3] = TerrainType::BRAMBLE;  // (3,2)
    ctx.set_grid(10, 10, tiles);
    register_builtin_content(ctx.catalog());
    const EntityId cat = cat_with(ctx, {5, 5, 5, 5, 5, 9, 5}, {2, 2}, 100);
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {8, 8});
    ctx.start_battle();

    const auto res = ctx.try_move(cat, {{2, 2}, {3, 2}});
    CHECK(res.ok);
    CHECK_EQ(ctx.unit(cat)->hp, 20 - 2);  // 进入荆棘 2 伤害
    CHECK_EQ(count_key(ctx.log(), "TerrainTriggered"), size_t(1));
}

// 水域：施加湿润；火球（fire 标签）移除湿润（元素交互）
TEST_CASE(terrain_water_wet_fire_interaction) {
    BattleContext ctx = make_battle(42);
    std::vector<TerrainType> tiles(10 * 10, TerrainType::EMPTY);
    tiles[2 * 10 + 3] = TerrainType::WATER;  // (3,2)
    ctx.set_grid(10, 10, tiles);
    register_builtin_content(ctx.catalog());
    const EntityId cat = cat_with(ctx, {5, 5, 5, 5, 5, 9, 5}, {2, 2}, 100);
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {8, 8});
    ctx.start_battle();

    CHECK(ctx.try_move(cat, {{2, 2}, {3, 2}}).ok);
    CHECK(ctx.status_system().has(cat, status_ids::kWet));

    // 施加燃烧（fire 标签）→ 湿润移除
    ctx.status_system().apply(cat, status_ids::kBurn, 1, kInvalidEntity, "t");
    CHECK(!ctx.status_system().has(cat, status_ids::kWet));
    CHECK(ctx.status_system().has(cat, status_ids::kBurn));
}

// 熔岩：进入 5 伤害（demo 地形）
TEST_CASE(terrain_lava) {
    BattleContext ctx = make_battle(42);
    std::vector<TerrainType> tiles(10 * 10, TerrainType::EMPTY);
    ctx.set_grid(10, 10, tiles);
    register_builtin_content(ctx.catalog());
    const EntityId cat = cat_with(ctx, {5, 5, 5, 5, 5, 9, 5}, {2, 2}, 100);
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {8, 8});
    ctx.start_battle();

    // 直接触发地形 API（熔岩不可通行，无法走进去）
    ctx.grid().tile({3, 2}).terrain = TerrainType::LAVA;
    ctx.apply_terrain_on_enter(cat, {3, 2});
    CHECK_EQ(ctx.unit(cat)->hp, 20 - 5);
}

// 花丛：驻留回合末额外回复（HP+MANA 各 +2，wiki 数值）
TEST_CASE(terrain_flower_regen) {
    BattleContext ctx = make_battle(42);
    std::vector<TerrainType> tiles(10 * 10, TerrainType::EMPTY);
    tiles[2 * 10 + 3] = TerrainType::FLOWER;
    ctx.set_grid(10, 10, tiles);
    register_builtin_content(ctx.catalog());
    const EntityId cat = cat_with(ctx, {5, 5, 5, 5, 5, 9, 5}, {2, 2}, 100);
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {8, 8});
    ctx.start_battle();

    ctx.try_move(cat, {{2, 2}, {3, 2}});
    // 造成伤害后驻留花丛回血
    DamageContext dmg;
    dmg.type = DamageType::TRUE;
    dmg.source = kInvalidEntity;
    dmg.target = cat;
    dmg.base_amount = 5;
    ctx.deal_damage(dmg);
    const int32_t hp_before = ctx.unit(cat)->hp;   // 20 - 5 = 15
    const int32_t mana_before = ctx.unit(cat)->mana;  // 5（无回蓝来源前）
    ctx.end_current_turn();
    CHECK_EQ(ctx.unit(cat)->hp, hp_before + 2);    // 花丛 +2 回血
    CHECK_EQ(ctx.unit(cat)->mana, mana_before + 5 + 2);  // INT 5 + 花丛 2
}
