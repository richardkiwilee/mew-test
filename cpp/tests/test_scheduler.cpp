#include "pch.h"
#include "test_framework.h"

#include "battle_test_utils.h"

using namespace mew::combat;
using namespace mewtest;

// 先攻降序：SPD 6 蝙蝠 > SPD 5 猫 > SPD 4 老鼠
TEST_CASE(scheduler_initiative_order) {
    BattleContext ctx = make_battle(42);
    const EntityId cat = spawn_cat(ctx, {5, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {5, 2});
    const EntityId bat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kBat),
                                        Team::ENEMY, {7, 5});
    ctx.start_battle();

    EntityId first = ctx.current_unit();
    CHECK_EQ(first, bat);  // init 12
    ctx.end_current_turn();
    ctx.begin_next_turn();
    CHECK_EQ(ctx.current_unit(), cat);  // init 10
    ctx.end_current_turn();
    ctx.begin_next_turn();
    CHECK_EQ(ctx.current_unit(), rat);  // init 8
    ctx.end_current_turn();
    ctx.begin_next_turn();
    // 新轮次：还是 bat 先
    CHECK_EQ(ctx.current_unit(), bat);
}

// 同先攻 tie-break：同 seed 多次开局顺序一致；三人顺序是 a/b/c 的某个排列
TEST_CASE(scheduler_tie_break_stable) {
    auto run = [] {
        BattleContext ctx = make_battle(1234);
        const EntityId a = spawn_cat(ctx, {5, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);
        const EntityId b = spawn_cat(ctx, {5, 5, 5, 5, 5, 5, 5}, {5, 2}, 101);
        const EntityId c = spawn_cat(ctx, {5, 5, 5, 5, 5, 5, 5}, {8, 2}, 102);
        ctx.start_battle();
        std::vector<EntityId> order;
        for (int i = 0; i < 3; ++i) {
            order.push_back(ctx.current_unit());
            ctx.end_current_turn();
            ctx.begin_next_turn();
        }
        // 是 {a,b,c} 的一个排列（各不相同且都出现）
        CHECK_EQ(order.size(), size_t(3));
        CHECK(order[0] != order[1] && order[1] != order[2] && order[0] != order[2]);
        CHECK(order[0] == a || order[0] == b || order[0] == c);
        CHECK(order[1] == a || order[1] == b || order[1] == c);
        CHECK(order[2] == a || order[2] == b || order[2] == c);
        return order;
    };
    CHECK_EQ(run(), run());  // 同 seed 稳定
}

// 死亡单位回合被跳过（TurnSkipped 日志）；战斗未结束（还有敌人存活）
TEST_CASE(scheduler_skips_dead) {
    BattleContext ctx = make_battle(7);
    const EntityId cat = spawn_cat(ctx, {5, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {5, 2});
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kBat), Team::ENEMY, {7, 5});
    ctx.start_battle();
    // 行动顺序：bat(12) → cat(10) → rat(8)；在猫的回合打死老鼠
    ctx.end_current_turn();               // bat 结束
    ctx.begin_next_turn();                // cat 开始
    CHECK_EQ(ctx.current_unit(), cat);
    DamageContext dmg;
    dmg.source = cat;
    dmg.target = rat;
    dmg.type = DamageType::TRUE;
    dmg.base_amount = 999;
    ctx.deal_damage(dmg);
    CHECK(!ctx.battle_over());            // 蝙蝠仍存活
    ctx.end_current_turn();
    ctx.begin_next_turn();                // rat 的回合被跳过 → 下一轮 bat
    CHECK(ctx.current_unit() != rat);
    CHECK(count_key(ctx.log(), "TurnSkipped") >= 1);
}

// 额外回合：紧接当前回合之后；每单位每轮上限 1（DD-004）
TEST_CASE(scheduler_extra_turn) {
    BattleContext ctx = make_battle(7);
    const EntityId cat = spawn_cat(ctx, {5, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {5, 2});
    ctx.start_battle();
    CHECK(ctx.scheduler().grant_extra_turn(cat, "test"));
    ctx.end_current_turn();
    ctx.begin_next_turn();
    CHECK_EQ(ctx.current_unit(), cat);
    CHECK_EQ(ctx.scheduler().last_turn_kind(), TurnKind::EXTRA);
    // 本轮内再给一次 → 拒绝（防无限）
    CHECK(!ctx.scheduler().grant_extra_turn(cat, "test2"));
    CHECK_EQ(count_key(ctx.log(), "ExtraTurnRejected"), size_t(1));
}

// 每轮一次主回合：猫先动 → 老鼠 → 新轮次再回猫（不因顺序重排重复获得主回合）
TEST_CASE(scheduler_no_duplicate_main_turn) {
    BattleContext ctx = make_battle(7);
    const EntityId cat = spawn_cat(ctx, {5, 5, 5, 5, 5, 8, 5}, {2, 2}, 100);
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {5, 2});
    ctx.start_battle();
    CHECK_EQ(ctx.current_unit(), cat);  // init 16 先动
    ctx.end_current_turn();
    ctx.begin_next_turn();
    CHECK_EQ(ctx.current_unit(), rat);  // init 8 后动
    ctx.end_current_turn();
    ctx.begin_next_turn();
    CHECK_EQ(ctx.current_unit(), cat);  // 新轮次：仍是猫先，但每轮只一次
    ctx.end_current_turn();
    ctx.begin_next_turn();
    CHECK_EQ(ctx.current_unit(), rat);
}
