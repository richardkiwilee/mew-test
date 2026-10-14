#include "pch.h"
#include "test_framework.h"

#include "battle_test_utils.h"

using namespace mew::combat;
using namespace mewtest;

namespace {

EntityId cat_with(BattleContext& ctx, const std::vector<uint32_t>& passives,
                  const std::vector<uint32_t>& abilities,
                  const std::array<int32_t, 7>& stats, Vec2i pos, uint32_t def_id) {
    UnitDef def = make_cat_def(def_id, stats, std::format("cat{}", def_id), passives);
    def.ability_ids = abilities;
    ctx.catalog().register_unit_def(def);
    return ctx.spawn_unit(def, Team::PLAYER, pos);
}

}  // namespace

// ---------------------------------------------------------------------------
// 技能施放：成本、范围、payload 按定义序执行（火球 = 5 魔法 + 燃烧 1）
// ---------------------------------------------------------------------------
TEST_CASE(ability_fireball_cost_and_payload) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    const EntityId cat = cat_with(ctx, {}, {ability_ids::kFireball},
                                  {5, 5, 5, 5, 8, 5, 5}, {2, 2}, 100);  // 法力 8
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {2, 5});  // 距离 3，射程内
    ctx.start_battle();
    const auto res = ctx.try_cast_ability(cat, ability_ids::kFireball, rat, rat == 0 ? Vec2i{} : ctx.unit(rat)->pos);
    CHECK(res.ok);
    CHECK_EQ(ctx.unit(cat)->mana, 8 - 3);          // 成本 3
    CHECK_EQ(ctx.unit(rat)->hp, 12 - 5);           // 5 魔法伤害
    CHECK_EQ(ctx.status_system().stacks_of(rat, status_ids::kBurn), 1);  // 燃烧 1
    CHECK_EQ(count_key(ctx.log(), "AbilityCastStarted"), size_t(1));
    CHECK_EQ(count_key(ctx.log(), "AbilityCastCompleted"), size_t(1));
}

// 校验：法力不足 / 超射程 / 不拥有的技能 / 目标非法
TEST_CASE(ability_validation) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    const EntityId cat = cat_with(ctx, {}, {ability_ids::kFireball},
                                  {5, 5, 5, 5, 1, 5, 5}, {2, 2}, 100);  // CHA 1 → 法力 1 < 3
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {2, 5});
    ctx.start_battle();

    CHECK_EQ(ctx.try_cast_ability(cat, ability_ids::kFireball, rat, ctx.unit(rat)->pos).reject,
             "not_enough_mana");

    // 补足法力：超射程（8 > 4）
    ctx.gain_resource(cat, ResourceType::MANA, 10);
    const EntityId far_rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                            Team::ENEMY, {8, 8});
    CHECK_EQ(ctx.try_cast_ability(cat, ability_ids::kFireball, far_rat, ctx.unit(far_rat)->pos).reject,
             "out_of_range");

    // 不拥有的技能
    CHECK_EQ(ctx.try_cast_ability(cat, ability_ids::kHeal, rat, ctx.unit(rat)->pos).reject,
             "ability_not_owned");

    // 友方目标非法（火球只能打敌人）——另一只猫
    const EntityId friend_cat = cat_with(ctx, {}, {}, {5, 5, 5, 5, 5, 5, 5}, {4, 2}, 101);
    CHECK_EQ(ctx.try_cast_ability(cat, ability_ids::kFireball, friend_cat, ctx.unit(friend_cat)->pos).reject,
             "invalid_target");
}

// 治疗技能：友方恢复
TEST_CASE(ability_heal_ally) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    const EntityId healer = cat_with(ctx, {}, {ability_ids::kHeal},
                                     {5, 5, 5, 5, 8, 5, 5}, {2, 2}, 100);
    const EntityId ally = cat_with(ctx, {}, {}, {5, 5, 5, 5, 5, 5, 5}, {3, 2}, 101);
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {8, 8});
    ctx.start_battle();

    DamageContext dmg;
    dmg.type = DamageType::TRUE;
    dmg.source = kInvalidEntity;
    dmg.target = ally;
    dmg.base_amount = 10;
    ctx.deal_damage(dmg);
    CHECK_EQ(ctx.unit(ally)->hp, 10);

    const auto res = ctx.try_cast_ability(healer, ability_ids::kHeal, ally, ctx.unit(ally)->pos);
    CHECK(res.ok);
    CHECK_EQ(ctx.unit(ally)->hp, 14);
    CHECK_EQ(ctx.unit(healer)->mana, 8 - 2);
}

// 施法消耗法力触发"法力学者"被动（事件链：ResourceSpent → 效果）
TEST_CASE(ability_synergy_with_scholar) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    const EntityId scholar = cat_with(ctx, {passive_ids::kManaScholar}, {},
                                      {5, 5, 5, 5, 8, 5, 5}, {2, 2}, 100);
    const EntityId caster = cat_with(ctx, {}, {ability_ids::kZap},
                                     {5, 5, 5, 5, 8, 7, 5}, {3, 2}, 101);  // SPD 7 先动
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {3, 6});  // 距离 4，Zap 射程内
    ctx.start_battle();
    CHECK_EQ(ctx.current_unit(), caster);

    // Zap 成本 2 < 7 → 学者不触发
    CHECK(ctx.try_cast_ability(caster, ability_ids::kZap, rat, ctx.unit(rat)->pos).ok);
    CHECK_EQ(ctx.unit(rat)->hp, 12 - 2);
    CHECK(!ctx.status_system().has(caster, status_ids::kAllStatsUp));
    (void)scholar;

    // 补充法力（成本 2 已扣除）
    ctx.gain_resource(caster, ResourceType::MANA, 10);
    CHECK_EQ(ctx.unit(caster)->mana, 8 - 2 + 10);
}

// 视线：墙后目标无法施放远程技能
TEST_CASE(ability_line_of_sight) {
    BattleContext ctx = make_battle(42);
    std::vector<TerrainType> tiles(10 * 10, TerrainType::EMPTY);
    tiles[4 * 10 + 2] = TerrainType::WALL;  // (2,4)：恰在 (2,2)→(2,5) 直线上
    ctx.set_grid(10, 10, tiles);
    register_builtin_content(ctx.catalog());
    const EntityId cat = cat_with(ctx, {}, {ability_ids::kFireball},
                                  {5, 5, 5, 5, 8, 5, 5}, {2, 2}, 100);
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {2, 5});
    ctx.start_battle();
    CHECK(!ctx.grid().has_line_of_sight({2, 2}, {2, 5}));
    CHECK_EQ(ctx.try_cast_ability(cat, ability_ids::kFireball, rat, ctx.unit(rat)->pos).reject,
             "no_line_of_sight");
}

// AoE：烈焰扫荡对半径 1 内所有敌人造成伤害
TEST_CASE(ability_aoe_radius) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    const EntityId cat = cat_with(ctx, {}, {ability_ids::kFlameSweep},
                                  {5, 5, 5, 5, 8, 5, 5}, {2, 2}, 100);
    const EntityId main = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                         Team::ENEMY, {2, 4});   // 距离 2（射程内）
    const EntityId near1 = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                          Team::ENEMY, {3, 4});  // 与 (2,4) 距离 1
    const EntityId far1 = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                         Team::ENEMY, {4, 4});   // 与 (2,4) 距离 2 → 不受
    ctx.start_battle();

    const auto res = ctx.try_cast_ability(cat, ability_ids::kFlameSweep, main, ctx.unit(main)->pos);
    CHECK(res.ok);
    CHECK_EQ(ctx.unit(main)->hp, 12 - 3);
    CHECK_EQ(ctx.unit(near1)->hp, 12 - 3);
    CHECK_EQ(ctx.unit(far1)->hp, 12);  // 半径外
}
