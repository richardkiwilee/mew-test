#include "pch.h"
#include "test_framework.h"

#include "battle_test_utils.h"

using namespace mew::combat;
using namespace mewtest;

namespace {

EntityId cat_with(BattleContext& ctx, const std::vector<uint32_t>& passives,
                  const std::array<int32_t, 7>& stats, Vec2i pos, uint32_t def_id) {
    UnitDef def = make_cat_def(def_id, stats, std::format("cat{}", def_id), passives);
    ctx.catalog().register_unit_def(def);
    return ctx.spawn_unit(def, Team::PLAYER, pos);
}

}  // namespace

// ---------------------------------------------------------------------------
// 反伤：受到伤害 → 反射 2 点真实伤害给来源
// ---------------------------------------------------------------------------
TEST_CASE(chain_reflect_damage) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    const EntityId cat = cat_with(ctx, {}, {8, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);  // 攻 3
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {3, 2});
    ctx.add_passive(rat, passive_ids::kThorns);
    ctx.start_battle();

    const int32_t cat_hp_before = ctx.unit(cat)->hp;
    const auto res = ctx.try_attack(cat, rat);
    CHECK(res.ok);
    CHECK_EQ(ctx.unit(rat)->hp, 12 - 3);       // 老鼠受 3 伤
    CHECK_EQ(ctx.unit(cat)->hp, cat_hp_before - 2);  // 反伤 2
    CHECK(count_key(ctx.log(), "DamageReflected") == 1);
}

// ---------------------------------------------------------------------------
// 反伤互杀（设计文档 §18.2 #4）：A 杀 B、B 反伤杀 A —— 不无限递归、战败
// ---------------------------------------------------------------------------
TEST_CASE(chain_reflect_mutual_kill) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    const EntityId cat = cat_with(ctx, {}, {8, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {3, 2});
    ctx.add_passive(rat, passive_ids::kThorns);
    ctx.start_battle();

    // 猫只剩 1 血：攻击 → 老鼠死（12−3 不足以死，先补刀）——用 TRUE 伤害把双方打到残血
    DamageContext setup;
    setup.type = DamageType::TRUE;
    setup.source = kInvalidEntity;
    setup.base_amount = 11;
    setup.target = rat;
    ctx.deal_damage(setup);          // 老鼠剩 1 血
    setup.target = cat;
    setup.base_amount = 19;
    ctx.deal_damage(setup);          // 猫剩 1 血

    const auto res = ctx.try_attack(cat, rat);
    CHECK(res.ok);
    // 猫 3 伤害击杀老鼠 → 反伤 2 击倒猫 → 敌方全灭 → 胜利判定优先（wiki：胜利=敌人全灭），
    // 倒地猫战后复活（post_battle_regen = 5）
    CHECK(ctx.unit(rat)->is_gone());
    CHECK(ctx.unit(cat)->is_alive());
    CHECK_EQ(ctx.unit(cat)->hp, 5);  // 被反伤击倒后由胜利结算复活
    CHECK(ctx.battle_over());
    CHECK_EQ(ctx.winner(), Team::PLAYER);
    CHECK_EQ(count_key(ctx.log(), "UnitDowned"), size_t(1));      // 反伤确实击倒过猫
    CHECK_EQ(count_key(ctx.log(), "UnitRevived"), size_t(1));     // 且被复活
    // 无无限递归：反伤只发生一次（反伤伤害带 reflect 标签不再触发），无 ChainLimitReached
    CHECK_EQ(count_key(ctx.log(), "DamageReflected"), size_t(1));
    CHECK_EQ(count_key(ctx.log(), "ChainLimitReached"), size_t(0));
}

// ---------------------------------------------------------------------------
// 击杀归属：最后有效伤害来源（设计文档 §7.2 OnKill / §12.1）
// ---------------------------------------------------------------------------
TEST_CASE(chain_kill_attribution_last_source) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    const EntityId a = cat_with(ctx, {passive_ids::kHunterReward}, {5, 5, 5, 5, 8, 5, 5}, {2, 2}, 100);
    const EntityId b = cat_with(ctx, {passive_ids::kHunterReward}, {5, 5, 5, 5, 8, 5, 5}, {3, 2}, 101);
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {4, 2});
    ctx.start_battle();

    const int32_t mana_a = ctx.unit(a)->mana;
    const int32_t mana_b = ctx.unit(b)->mana;
    // A 先打 5，B 补刀击杀 → 奖励只归 B
    DamageContext dmg;
    dmg.type = DamageType::TRUE;
    dmg.target = rat;
    dmg.source = a;
    dmg.base_amount = 5;
    ctx.deal_damage(dmg);
    dmg.source = b;
    dmg.base_amount = 999;
    ctx.deal_damage(dmg);

    CHECK(ctx.unit(rat)->is_gone());
    CHECK_EQ(ctx.unit(a)->mana, mana_a);      // A 无奖励
    CHECK_EQ(ctx.unit(b)->mana, mana_b + 3);  // B +3 法力
    CHECK_EQ(ctx.status_system().stacks_of(b, status_ids::kStrengthUp), 1);
}

// ---------------------------------------------------------------------------
// DoT 击杀归属：燃烧伤害的来源 = 状态施加者
// ---------------------------------------------------------------------------
TEST_CASE(chain_dot_kill_attribution) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    const EntityId cat = cat_with(ctx, {passive_ids::kHunterReward}, {5, 5, 5, 5, 8, 5, 5}, {2, 2}, 100);
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {8, 8});
    ctx.start_battle();

    // 把老鼠打到 1 血（环境伤害），然后猫施加燃烧 1
    DamageContext dmg;
    dmg.type = DamageType::TRUE;
    dmg.source = kInvalidEntity;
    dmg.target = rat;
    dmg.base_amount = 11;
    ctx.deal_damage(dmg);
    ctx.status_system().apply(rat, status_ids::kBurn, 1, cat, "test");

    ctx.end_current_turn();   // 猫结束（含回蓝）
    const int32_t mana_before = ctx.unit(cat)->mana;
    ctx.begin_next_turn();    // 老鼠回合开始：燃烧 1 伤害致死 → 击杀归属 = 猫
    CHECK(ctx.unit(rat)->is_gone());
    CHECK_EQ(ctx.unit(cat)->mana, mana_before + 3);
}

// ---------------------------------------------------------------------------
// 复活（统一 API，DD-007/§12.2）：保留实体 ID、本轮不行动、下轮入队
// ---------------------------------------------------------------------------
TEST_CASE(chain_revive_unit) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    const EntityId cat = cat_with(ctx, {}, {5, 5, 5, 5, 5, 9, 5}, {2, 2}, 100);  // init 18 先动
    cat_with(ctx, {}, {5, 5, 5, 5, 5, 5, 5}, {4, 4}, 101);                       // 保底存活
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {8, 8});
    ctx.start_battle();

    DamageContext dmg;
    dmg.type = DamageType::TRUE;
    dmg.source = kInvalidEntity;
    dmg.target = cat;
    dmg.base_amount = 999;
    ctx.deal_damage(dmg);
    CHECK(ctx.unit(cat)->is_down());
    CHECK(!ctx.battle_over());  // 另一只猫仍存活

    CHECK(ctx.revive_unit(cat, 10, "test"));
    CHECK(ctx.unit(cat)->is_alive());
    CHECK_EQ(ctx.unit(cat)->hp, 10);
    CHECK_EQ(count_key(ctx.log(), "UnitRevived"), size_t(1));
    // 复活当轮不再行动：猫 A 结束 → 猫 B（init 10）→ 老鼠（init 8）→ 下一轮 A
    ctx.end_current_turn();
    ctx.begin_next_turn();
    CHECK(ctx.current_unit() != cat);  // A 本轮已行动过
    ctx.end_current_turn();
    ctx.begin_next_turn();
    ctx.end_current_turn();
    ctx.begin_next_turn();  // 下一轮：A（init 18 先动）
    CHECK_EQ(ctx.current_unit(), cat);
}

// ---------------------------------------------------------------------------
// 死亡召唤（§12.3）：击杀后召唤；无空格时失败且不崩溃
// ---------------------------------------------------------------------------
TEST_CASE(chain_death_spawn) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    // 测试被动：击杀时召唤一只老鼠
    PassiveDef spawn_on_kill;
    spawn_on_kill.id = 950;
    spawn_on_kill.display_name = "死灵召唤";
    EffectDef eff;
    eff.id = 1;
    eff.type = EffectType::SPAWN_UNIT;
    eff.trigger = EventType::UnitDied;
    eff.phase = EventPhase::AFTER;
    eff.priority = 0;
    eff.condition = ConditionNode::make(ConditionOp::OWNER_IS_LAST_EFFECTIVE_DAMAGE_SOURCE);
    eff.target = TargetSelectorType::SELF;
    eff.unit_def_id = unit_ids::kRat;
    eff.limit_scope = TriggerLimitScope::PER_EVENT;
    eff.limit_count = 1;
    spawn_on_kill.effects.push_back(std::move(eff));
    ctx.catalog().register_passive_def(std::move(spawn_on_kill));

    const EntityId cat = cat_with(ctx, {950}, {8, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {3, 2});
    ctx.start_battle();

    // 先把老鼠打到 1 血，再由猫击杀（击杀归属 = 猫）
    DamageContext setup;
    setup.type = DamageType::TRUE;
    setup.source = kInvalidEntity;
    setup.target = rat;
    setup.base_amount = 11;
    ctx.deal_damage(setup);
    CHECK_EQ(ctx.unit(rat)->hp, 1);

    const size_t units_before = ctx.units().size();
    const auto res = ctx.try_attack(cat, rat);
    CHECK(res.ok);
    CHECK(ctx.unit(rat)->is_gone());
    CHECK_EQ(ctx.units().size(), units_before + 1);  // 召唤了 1 只
    CHECK_EQ(count_key(ctx.log(), "UnitSummoned"), size_t(1));
    // 召唤物下轮入队行动
    const EntityId summoned = static_cast<EntityId>(units_before + 1);
    CHECK_EQ(ctx.unit(summoned)->team, Team::ENEMY);
}

TEST_CASE(chain_death_spawn_no_free_tile) {
    // 3×3 小棋盘塞满：无空格 → SpawnFailed，不崩溃
    BattleContext ctx = make_battle(42, 3, 3);
    register_builtin_content(ctx.catalog());
    PassiveDef spawn_on_kill;
    spawn_on_kill.id = 951;
    spawn_on_kill.display_name = "死灵召唤2";
    EffectDef eff;
    eff.id = 1;
    eff.type = EffectType::SPAWN_UNIT;
    eff.trigger = EventType::UnitDied;
    eff.phase = EventPhase::AFTER;
    eff.condition = ConditionNode::make(ConditionOp::OWNER_IS_LAST_EFFECTIVE_DAMAGE_SOURCE);
    eff.target = TargetSelectorType::SELF;
    eff.unit_def_id = unit_ids::kRat;
    eff.limit_scope = TriggerLimitScope::PER_EVENT;
    eff.limit_count = 1;
    spawn_on_kill.effects.push_back(std::move(eff));
    ctx.catalog().register_passive_def(std::move(spawn_on_kill));

    // 杀手在 (0,0)；其余 7 只猫占满 (0,0) 半径 3 内所有格子；老鼠在 (2,2)（半径 4）
    uint32_t def_id = 100;
    EntityId killer = cat_with(ctx, {951}, {8, 5, 5, 5, 5, 5, 5}, {0, 0}, def_id++);
    for (int32_t y = 0; y < 3; ++y) {
        for (int32_t x = 0; x < 3; ++x) {
            if (x == 0 && y == 0) continue;
            if (x == 2 && y == 2) continue;  // 老鼠位
            cat_with(ctx, {}, {5, 5, 5, 5, 5, 5, 5}, {x, y}, def_id++);
        }
    }
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {2, 2});
    ctx.start_battle();

    DamageContext dmg;
    dmg.type = DamageType::TRUE;
    dmg.source = killer;
    dmg.target = rat;
    dmg.base_amount = 999;
    ctx.deal_damage(dmg);
    CHECK(ctx.unit(rat)->is_gone());
    // 老鼠死后其格子虽空出，但在召唤搜索半径（3）之外 → SpawnFailed
    CHECK_EQ(count_key(ctx.log(), "SpawnFailed"), size_t(1));  // 无可用格：失败但稳定
    CHECK(count_key(ctx.log(), "ChainLimitReached") == 0);
}
