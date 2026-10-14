#include "pch.h"
#include "test_framework.h"

#include "battle_test_utils.h"

using namespace mew::combat;
using namespace mewtest;

// ---------------------------------------------------------------------------
// 敌人 AI：最近目标选择、移动逼近、普攻、施法（意图全部经 ActionResolver 验证）
// ---------------------------------------------------------------------------
TEST_CASE(ai_melee_enemy_approaches_and_attacks) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    spawn_cat(ctx, {5, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {7, 2});
    ctx.start_battle();

    // 双方 SPD 5/4：猫先动。猫跳过 → 老鼠 AI 行动
    ctx.end_current_turn();
    ctx.begin_next_turn();
    CHECK_EQ(ctx.current_unit(), rat);
    ctx.run_ai_turn();
    // 老鼠应移动逼近（从 (7,2) 向 (2,2)）
    CHECK(GridMap::manhattan(ctx.unit(rat)->pos, {2, 2}) < 5);
    CHECK(count_key(ctx.log(), "AiMove") >= 1);
}

// AI 施法：火妖在射程内放火球（消耗法力、造成伤害+燃烧）
TEST_CASE(ai_casts_fireball_in_range) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    const EntityId cat = spawn_cat(ctx, {5, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);
    const EntityId imp = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kImp),
                                        Team::ENEMY, {2, 5});  // 距离 3（火球射程 2-4）
    ctx.start_battle();

    // 火妖 SPD 6（init 12）> 猫 10：火妖先动
    CHECK_EQ(ctx.current_unit(), imp);
    ctx.run_ai_turn();
    CHECK_EQ(ctx.unit(imp)->mana, 3 - 3 + 5);   // 法力 3 → 0，回合末回蓝 5（INT 5）
    CHECK_EQ(ctx.unit(cat)->hp, 20 - 5 - 1);    // 火球 5 + 猫回合开始燃烧 tick 1
    CHECK_EQ(ctx.status_system().stacks_of(cat, status_ids::kBurn), 0);  // tick 后衰减归零
    CHECK_EQ(count_key(ctx.log(), "AiCast"), size_t(1));
}

// AI 普攻：相邻时直接打
TEST_CASE(ai_melee_attacks_adjacent) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    const EntityId cat = spawn_cat(ctx, {5, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {3, 2});
    ctx.start_battle();

    ctx.end_current_turn();  // 猫跳过
    ctx.begin_next_turn();   // 老鼠
    ctx.run_ai_turn();
    CHECK_EQ(ctx.unit(cat)->hp, 20 - 1);  // 老鼠 STR 6 → 攻 1
    CHECK_EQ(count_key(ctx.log(), "AiAttack"), size_t(1));
}

// 全 AI 战斗确定性：固定 seed 完全一致（设计文档 §19 阶段 1 验收）
TEST_CASE(ai_full_battle_determinism) {
    auto run = [] {
        BattleContext ctx = make_battle(20261014);
        register_builtin_content(ctx.catalog());
        spawn_cat(ctx, {8, 5, 6, 5, 5, 5, 5}, {2, 2}, 100);
        spawn_cat(ctx, {5, 6, 5, 5, 8, 6, 5}, {2, 3}, 101);
        spawn_cat(ctx, {5, 5, 7, 5, 5, 5, 6}, {3, 2}, 102);
        spawn_cat(ctx, {5, 5, 5, 7, 5, 7, 5}, {3, 3}, 103);
        ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {7, 2});
        ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kImp), Team::ENEMY, {7, 5});
        ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kSpitter), Team::ENEMY, {8, 8});
        ctx.start_battle();
        // 双方都走 AI：玩家方猫也用脚本（简单近战/逼近），敌方走 EnemyAi
        for (int i = 0; i < 3000 && !ctx.battle_over(); ++i) {
            const EntityId cur = ctx.current_unit();
            if (cur == kInvalidEntity) break;
            const UnitRuntime* u = ctx.unit(cur);
            if (u->team == Team::ENEMY) {
                ctx.run_ai_turn();
            } else {
                run_scripted_ai(ctx);
                ctx.end_current_turn();
                ctx.begin_next_turn();
            }
        }
        return ctx.log().to_text();
    };
    CHECK_EQ(run(), run());
}
