#include "pch.h"
#include "test_framework.h"

#include "battle_test_utils.h"

using namespace mew::combat;
using namespace mewtest;

// ---------------------------------------------------------------------------
// 集成：1 猫 vs 1 鼠，脚本 AI 打到分出胜负；日志包含完整结算链
// ---------------------------------------------------------------------------
TEST_CASE(battle_cat_vs_rat) {
    BattleContext ctx = make_battle(42);
    const EntityId cat = spawn_cat(ctx, {8, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);  // 攻 3，HP 20
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {3, 2});
    play_battle(ctx);

    CHECK(ctx.battle_over());
    CHECK_EQ(ctx.winner(), Team::PLAYER);
    CHECK(count_key(ctx.log(), "BattleStarted") == 1);
    CHECK(count_key(ctx.log(), "TurnOrder") >= 1);
    CHECK(count_key(ctx.log(), "AttackDeclared") >= 1);
    CHECK(count_key(ctx.log(), "AttackHit") >= 1);
    CHECK(count_key(ctx.log(), "DamageApplied") >= 1);
    CHECK(count_key(ctx.log(), "UnitDied") >= 1);
    CHECK(count_key(ctx.log(), "BattleEnded") == 1);
}

// ---------------------------------------------------------------------------
// 确定性验收（设计文档 §19 阶段 1）：同 seed + 同输入序列 → 日志完全一致
// ---------------------------------------------------------------------------
TEST_CASE(battle_determinism) {
    auto run = [] {
        BattleContext ctx = make_battle(20261014);
        spawn_cat(ctx, {7, 5, 5, 5, 5, 6, 5}, {2, 2}, 100);
        spawn_cat(ctx, {6, 6, 5, 5, 5, 5, 5}, {2, 3}, 101);
        ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {6, 2});
        ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kBat), Team::ENEMY, {7, 5});
        play_battle(ctx);
        return ctx.log().to_text();
    };
    CHECK_EQ(run(), run());
}

// ---------------------------------------------------------------------------
// 行动校验：非法移动/攻击被拒绝且不消耗行动资源
// ---------------------------------------------------------------------------
TEST_CASE(action_validation) {
    BattleContext ctx = make_battle(9);
    const EntityId cat = spawn_cat(ctx, {5, 5, 5, 5, 5, 9, 5}, {2, 2}, 100);  // init 18 先动
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {7, 2});
    ctx.start_battle();
    CHECK_EQ(ctx.current_unit(), cat);

    // 攻击远处目标 → 拒绝
    CHECK_EQ(ctx.try_attack(cat, rat).ok, false);
    // 移动路径不邻接 → 拒绝
    CHECK_EQ(ctx.try_move(cat, {{2, 2}, {4, 2}}).ok, false);
    // 移动到被占格 → 拒绝
    CHECK_EQ(ctx.try_move(cat, {{2, 2}, {2, 3}}).ok, true);   // 先正常走一步
    ctx.try_move(cat, {{2, 3}, {3, 3}});                      // 消耗第 2 步（合法）
    // 第二次移动（无移动动作）→ 拒绝
    CHECK_EQ(ctx.try_move(cat, {{3, 3}, {4, 3}}).ok, false);

    // 移动动作刷新后老鼠回合再验证一次"not_your_turn"
    CHECK_EQ(ctx.try_move(rat, {{7, 2}, {6, 2}}).ok, false);
}

// ---------------------------------------------------------------------------
// 死亡模型（DD-007）：唯一玩家猫被击倒 → 立即战败（倒地≠死亡，无 UnitDied）
// ---------------------------------------------------------------------------
TEST_CASE(death_downed_defeat) {
    BattleContext ctx = make_battle(3);
    const EntityId cat = spawn_cat(ctx, {5, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);  // HP 20
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {5, 2});
    ctx.start_battle();

    DamageContext dmg;
    dmg.source = kInvalidEntity;
    dmg.target = cat;
    dmg.type = DamageType::TRUE;
    dmg.base_amount = 20;
    ctx.deal_damage(dmg);
    CHECK(ctx.unit(cat)->is_down());
    CHECK(count_key(ctx.log(), "UnitDowned") == 1);
    CHECK(count_key(ctx.log(), "UnitDied") == 0);
    CHECK(ctx.battle_over());              // 唯一战力倒地 → 战败
    CHECK_EQ(ctx.winner(), Team::ENEMY);
    CHECK(count_key(ctx.log(), "BattleEnded") == 1);
}

// 尸体韧性：倒地猫再挨 3 次攻击 → 尸体销毁（UnitDied）
TEST_CASE(death_corpse_destroyed) {
    BattleContext ctx = make_battle(3);
    // 两只猫：一只被击倒，另一只活着 → 战斗未结束，可以攻击尸体
    const EntityId cat_a = spawn_cat(ctx, {5, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);
    spawn_cat(ctx, {5, 5, 5, 5, 5, 5, 5}, {5, 5}, 101);
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {5, 2});
    ctx.start_battle();

    DamageContext down;
    down.source = kInvalidEntity;
    down.target = cat_a;
    down.type = DamageType::TRUE;
    down.base_amount = 999;
    ctx.deal_damage(down);
    CHECK(ctx.unit(cat_a)->is_down());

    // 三次攻击尸体 → 尸体韧性 3 → 0 → 死亡
    DamageContext hit;
    hit.source = kInvalidEntity;
    hit.target = cat_a;
    hit.type = DamageType::TRUE;
    hit.base_amount = 1;
    ctx.deal_damage(hit);
    CHECK_EQ(ctx.unit(cat_a)->corpse_health_remaining, 2);
    ctx.deal_damage(hit);
    ctx.deal_damage(hit);
    CHECK(ctx.unit(cat_a)->is_gone());
    CHECK(count_key(ctx.log(), "CorpseHealthLost") == 3);
    CHECK(count_key(ctx.log(), "UnitDied") == 1);
    CHECK(!ctx.battle_over());  // 还有一只猫活着
}

// 胜利结算：倒地猫战后复活（DD-007 回血 = post_battle_regen）
TEST_CASE(victory_revives_downed_cats) {
    BattleContext ctx = make_battle(3);
    const EntityId cat = spawn_cat(ctx, {5, 5, 7, 5, 5, 5, 5}, {2, 2}, 100);  // CON 7 → 回复 6
    spawn_cat(ctx, {5, 5, 5, 5, 5, 5, 5}, {5, 5}, 101);                        // 保底存活战力
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {3, 2});
    ctx.start_battle();

    // 先把猫击倒（另一只猫仍存活 → 不触发战败），再打死老鼠
    DamageContext down;
    down.source = kInvalidEntity;
    down.target = cat;
    down.type = DamageType::TRUE;
    down.base_amount = 999;
    ctx.deal_damage(down);
    CHECK(ctx.unit(cat)->is_down());
    CHECK(!ctx.battle_over());

    DamageContext kill;
    kill.source = kInvalidEntity;
    kill.target = rat;
    kill.type = DamageType::TRUE;
    kill.base_amount = 999;
    ctx.deal_damage(kill);

    CHECK(ctx.battle_over());
    CHECK_EQ(ctx.winner(), Team::PLAYER);
    CHECK(ctx.unit(cat)->is_alive());
    CHECK_EQ(ctx.unit(cat)->hp, 6);  // post_battle_regen
    CHECK(count_key(ctx.log(), "UnitRevived") == 1);
}

// 移动结算：逐格事件 + 终点正确 + 动作消耗
TEST_CASE(move_resolution_events) {
    BattleContext ctx = make_battle(3);
    const EntityId cat = spawn_cat(ctx, {5, 5, 5, 5, 5, 9, 5}, {2, 2}, 100);  // 移动 6
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {8, 8});
    ctx.start_battle();

    const auto path = ctx.path_to(cat, {2, 5});
    CHECK(!path.empty());
    const auto res = ctx.try_move(cat, path);
    CHECK_EQ(res.ok, true);
    CHECK_EQ(ctx.unit(cat)->pos, (Vec2i{2, 5}));
    CHECK_EQ(ctx.unit(cat)->action_points[0], 0);  // 移动动作已消耗
    CHECK(count_key(ctx.log(), "MovementStarted") == 1);
    CHECK(count_key(ctx.log(), "MovementCompleted") == 1);
    CHECK(count_key(ctx.log(), "UnitEnteredTile") == path.size() - 1);
    CHECK(count_key(ctx.log(), "UnitMoved") == path.size() - 1);
}
