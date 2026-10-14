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
// 幸运 +21（设计文档 §15.1）：装备/被动阶段常驻修正
// ---------------------------------------------------------------------------
TEST_CASE(stats_lucky_plus_21) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    const EntityId cat = cat_with(ctx, {passive_ids::kSuperLuck}, {5, 5, 5, 5, 5, 5, 4}, {2, 2}, 100);
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {6, 6});
    ctx.start_battle();

    CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::LCK), 25);   // 4 + 21
    CHECK_EQ(ctx.stats().crit_chance_bp(ctx, cat), 5000);                // 2×25×100 bp = 50%
    CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::STR), 5);    // 其他属性不受影响
}

// ---------------------------------------------------------------------------
// 极值交换（设计文档 §9.3/§15.2 验收，§19 阶段 3）：
// 基础 {STR=8,DEX=7,INT=6,CHA=5,LCK=4} + 幸运+21 → {8,7,6,5,25}
// → 交换最大/最小 → {8,7,6,25,5}；重算绝不二次交换
// ---------------------------------------------------------------------------
TEST_CASE(stats_swap_extremes_once) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    // 数组按枚举序 STR/DEX/CON/INT/CHA/SPD/LCK：
    // STR=8 DEX=7 CON=6 INT=6 CHA=5 SPD=6 LCK=4（CHA 为唯一最小值，与设计文档示例一致）
    const EntityId cat = cat_with(ctx, {passive_ids::kSuperLuck, passive_ids::kSwapExtremes},
                                  {8, 7, 6, 6, 5, 6, 4}, {2, 2}, 100);
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {6, 6});
    ctx.start_battle();

    // 交换后：CHA=25（来自 LCK 的 4+21），LCK=5（来自 CHA 的 5）
    CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::STR), 8);
    CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::DEX), 7);
    CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::INT), 6);
    CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::CHA), 25);
    CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::LCK), 5);

    // 重复计算多次：不二次交换（设计文档 §0 规则 8）
    for (int i = 0; i < 5; ++i) {
        CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::CHA), 25);
        CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::LCK), 5);
        CHECK_EQ(ctx.stats().max_mana(ctx, cat), 75);  // CHA 25 × 3
    }
    // TransformApplied 只记录一次
    CHECK_EQ(count_key(ctx.log(), "TransformApplied"), size_t(1));

    // 战中施加减益不引发二次交换：weakness（STR/DEX/SPD −2）只影响对应属性
    ctx.status_system().apply(cat, status_ids::kWeakness, 1, kInvalidEntity, "test");
    CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::STR), 6);
    CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::DEX), 5);
    CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::SPD), 4);  // 6 − 2
    CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::CHA), 25);  // 交换结果稳定
    CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::LCK), 5);
}

// ---------------------------------------------------------------------------
// 修正顺序（README 核心关注点）：常驻被动（烘焙）→ 战中加入的状态（动态叠加）
// ---------------------------------------------------------------------------
TEST_CASE(stats_modifier_ordering) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    const EntityId cat = cat_with(ctx, {passive_ids::kBrave}, {5, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {6, 6});
    ctx.start_battle();

    CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::STR), 7);   // 5 + 2（勇敢，烘焙）
    // 战中叠加 strength_up ×2 → +4
    ctx.status_system().apply(cat, status_ids::kStrengthUp, 2, kInvalidEntity, "t");
    CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::STR), 11);  // 5 + 2 + 4
    CHECK_EQ(ctx.stats().melee_attack_damage(ctx, cat), 6);             // 11 − 5
    // 移除状态 → 回到常驻值（状态修正声明式、可逆）
    ctx.status_system().remove(cat, status_ids::kStrengthUp, "t");
    CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::STR), 7);
}

// ---------------------------------------------------------------------------
// 派生属性跟随有效值：敏捷（SPD+4 → 移动 +2）、虚弱（SPD−2 → 移动 −1）
// ---------------------------------------------------------------------------
TEST_CASE(stats_derived_follow_effective) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    const EntityId cat = cat_with(ctx, {passive_ids::kAgile}, {5, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {6, 6});
    ctx.start_battle();

    CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::SPD), 9);   // 5 + 4
    CHECK_EQ(ctx.stats().move_range(ctx, cat), 6);                      // 4 + trunc(4/2)
    CHECK_EQ(ctx.stats().initiative(ctx, cat), 18);                     // 2×9

    ctx.status_system().apply(cat, status_ids::kWeakness, 1, kInvalidEntity, "t");
    CHECK_EQ(ctx.stats().effective_stat(ctx, cat, StatType::SPD), 7);   // 9 − 2
    CHECK_EQ(ctx.stats().move_range(ctx, cat), 5);                      // 4 + trunc(2/2)
    CHECK_EQ(ctx.stats().initiative(ctx, cat), 14);
    // 最大法力/生命跟随属性变化（不改变当前值，只改上限）
    const int32_t hp_before = ctx.unit(cat)->hp;
    ctx.status_system().apply(cat, status_ids::kAllStatsUp, 1, kInvalidEntity, "t");
    CHECK_EQ(ctx.stats().max_hp(ctx, cat), 24);  // CON 6 × 4
    CHECK_EQ(ctx.unit(cat)->hp, hp_before);      // 当前值不变
}

// ---------------------------------------------------------------------------
// 先攻动态重排：速度变化影响未开始单位，不影响已完成单位（设计文档 §14.1）
// ---------------------------------------------------------------------------
TEST_CASE(stats_initiative_reorder_pending) {
    BattleContext ctx = make_battle(42);
    register_builtin_content(ctx.catalog());
    // A、B 同 SPD 5（init 10，tie-break 决定先后）；C SPD 4（init 8）
    const EntityId a = cat_with(ctx, {}, {5, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);
    const EntityId b = cat_with(ctx, {}, {5, 5, 5, 5, 5, 5, 5}, {3, 3}, 101);
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {6, 6});  // C: SPD 4
    ctx.start_battle();

    const EntityId first = ctx.current_unit();
    CHECK(first == a || first == b);
    // 在第一个单位回合中给另一个 SPD5 的猫上虚弱 → 其先攻降至 6 < 老鼠 8
    const EntityId other = first == a ? b : a;
    ctx.status_system().apply(other, status_ids::kWeakness, 1, kInvalidEntity, "t");
    ctx.end_current_turn();
    ctx.begin_next_turn();
    // 老鼠（init 8）先于被虚弱的猫（init 6）
    const EntityId rat = ctx.units().back().entity_id;
    CHECK_EQ(ctx.current_unit(), rat);
    (void)first;
    (void)other;
}
