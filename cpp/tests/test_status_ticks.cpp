#include "pch.h"
#include "test_framework.h"

#include "battle_test_utils.h"

using namespace mew::combat;
using namespace mewtest;

namespace {

// 猫(SPD 5) + 远处老鼠(SPD 4)：猫先动
struct BurnFixture {
    BattleContext ctx;
    EntityId cat;
    EntityId rat;

    explicit BurnFixture(uint64_t seed = 42) : ctx(make_battle(seed)) {
        register_builtin_content(ctx.catalog());
        cat = spawn_cat(ctx, {5, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);
        rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {8, 8});
    }
    void start() { ctx.start_battle(); }
    // 结束当前回合并进入下一个（越过老鼠回合直到回到猫，或直接走一格）
    void end_turn() {
        ctx.end_current_turn();
        ctx.begin_next_turn();
    }
};

}  // namespace

// Burn：回合开始每层 1 伤害（无视减免），然后 −1 层，归零过期（wiki 时机对齐）
TEST_CASE(status_burn_ticks_and_decays) {
    BurnFixture f;
    f.ctx.status_system().apply(f.cat, status_ids::kBurn, 3, kInvalidEntity, "t");
    f.start();  // 猫回合开始 → tick#1：3 伤害，层数 2
    CHECK_EQ(f.ctx.unit(f.cat)->hp, 17);
    CHECK_EQ(f.ctx.status_system().stacks_of(f.cat, status_ids::kBurn), 2);

    f.end_turn();  // 老鼠回合（无 tick）
    f.end_turn();  // 猫回合开始 → tick#2：2 伤害，层数 1
    CHECK_EQ(f.ctx.unit(f.cat)->hp, 15);
    CHECK_EQ(f.ctx.status_system().stacks_of(f.cat, status_ids::kBurn), 1);

    f.end_turn();
    f.end_turn();  // tick#3：1 伤害，层数 0 → 过期
    CHECK_EQ(f.ctx.unit(f.cat)->hp, 14);
    CHECK(!f.ctx.status_system().has(f.cat, status_ids::kBurn));
    CHECK(count_key(f.ctx.log(), "StatusExpired") >= 1);
    CHECK(count_key(f.ctx.log(), "StatusTicked") >= 3);
}

// Poison：回合开始每层 1 伤害，不衰减
TEST_CASE(status_poison_no_decay) {
    BurnFixture f;
    f.ctx.status_system().apply(f.cat, status_ids::kPoison, 2, kInvalidEntity, "t");
    f.start();
    CHECK_EQ(f.ctx.unit(f.cat)->hp, 18);
    CHECK_EQ(f.ctx.status_system().stacks_of(f.cat, status_ids::kPoison), 2);
    f.end_turn();
    f.end_turn();
    CHECK_EQ(f.ctx.unit(f.cat)->hp, 16);
    CHECK_EQ(f.ctx.status_system().stacks_of(f.cat, status_ids::kPoison), 2);  // 仍 2 层
}

// Bleed：轮末受 =层数 伤害，然后 +1 层（逐轮成长）
TEST_CASE(status_bleed_round_end_growth) {
    BurnFixture f;
    f.ctx.status_system().apply(f.cat, status_ids::kBleed, 1, kInvalidEntity, "t");
    f.start();
    CHECK_EQ(f.ctx.unit(f.cat)->hp, 20);  // 第 1 轮内无伤害
    f.end_turn();  // 猫结束
    f.end_turn();  // 老鼠结束 → 轮末：1 伤害，层数 2
    CHECK_EQ(f.ctx.unit(f.cat)->hp, 19);
    CHECK_EQ(f.ctx.status_system().stacks_of(f.cat, status_ids::kBleed), 2);
    f.end_turn();
    f.end_turn();  // 轮 2 末：2 伤害，层数 3
    CHECK_EQ(f.ctx.unit(f.cat)->hp, 17);
    CHECK_EQ(f.ctx.status_system().stacks_of(f.cat, status_ids::kBleed), 3);
}

// Stun：跳过回合（DD-019），且时长在跳过的回合末推进（不会永久眩晕）
TEST_CASE(status_stun_skips_turn) {
    BurnFixture f;
    f.ctx.status_system().apply(f.cat, status_ids::kStun, 1, kInvalidEntity, "t");
    f.start();
    // 猫被眩晕 → 直接到老鼠
    CHECK_EQ(f.ctx.current_unit(), f.rat);
    CHECK(count_key(f.ctx.log(), "TurnSkipped") >= 1);
    CHECK(!f.ctx.status_system().has(f.cat, status_ids::kStun));  // 时长已耗尽
    f.end_turn();  // 老鼠结束 → 下一轮猫可正常行动
    CHECK_EQ(f.ctx.current_unit(), f.cat);
}

// DD-008 off-by-one：施加当回合不计时长，下一规定时机起减
TEST_CASE(status_duration_no_off_by_one) {
    BurnFixture f;
    f.start();  // 猫回合 1 已开始
    // 猫回合 1 中施加 all_stats_up（duration 1）
    f.ctx.status_system().apply(f.cat, status_ids::kAllStatsUp, 1, kInvalidEntity, "t");
    f.end_turn();  // 猫回合 1 结束：施加回合不计 → 状态保留
    CHECK(f.ctx.status_system().has(f.cat, status_ids::kAllStatsUp));
    CHECK_EQ(f.ctx.unit(f.cat)->hp, 20);   // 无回血来源 → HP 不变（CON 6 只提高上限到 24）
    CHECK_EQ(f.ctx.unit(f.cat)->mana, 11); // INT 5+1=6 → 回合末回蓝 6（起始 CHA=5，5+6）
    f.end_turn();  // 老鼠回合
    f.end_turn();  // 猫回合 2 结束：时长 −1 → 0 → 过期
    CHECK(!f.ctx.status_system().has(f.cat, status_ids::kAllStatsUp));
    CHECK_EQ(count_key(f.ctx.log(), "StatusExpired"), size_t(1));
}

// 回合开始 DoT 致死：取消回合（设计文档 §18.2 #7）
TEST_CASE(status_burn_kills_at_turn_start) {
    BurnFixture f;
    // 猫只有 3 血
    f.ctx.deal_damage(DamageContext{.source = kInvalidEntity, .target = f.cat,
                                    .type = DamageType::TRUE, .base_amount = 17});
    f.ctx.status_system().apply(f.cat, status_ids::kBurn, 3, kInvalidEntity, "t");
    f.start();  // 猫回合开始：3 伤害 → 倒地 → 战败（唯一猫）
    CHECK(f.ctx.unit(f.cat)->is_down());
    CHECK(f.ctx.battle_over());
    CHECK_EQ(f.ctx.winner(), Team::ENEMY);
}

// DoT 全程确定性（同 seed 同日志）
TEST_CASE(status_dot_determinism) {
    auto run = [] {
        BurnFixture f(2026);
        f.ctx.status_system().apply(f.rat, status_ids::kBurn, 2, kInvalidEntity, "t");
        f.ctx.status_system().apply(f.cat, status_ids::kPoison, 1, kInvalidEntity, "t");
        play_battle(f.ctx);
        return f.ctx.log().to_text();
    };
    CHECK_EQ(run(), run());
}
