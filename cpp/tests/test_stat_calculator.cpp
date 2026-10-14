#include "pch.h"
#include "test_framework.h"

#include "combat/domain/unit_def.h"
#include "combat/stats/stat_calculator.h"

using namespace mew::combat;

namespace {

UnitDef make_def(const std::array<int32_t, kStatCount>& stats, bool can_crit = true,
                 int32_t base_move = 4) {
    UnitDef def;
    def.base_stats = stats;
    def.can_crit = can_crit;
    def.base_move = base_move;
    return def;
}

}  // namespace

// wiki 公式：MAX_HP = CON×4（最低 1）
TEST_CASE(stats_max_hp) {
    StatCalculator calc;
    CHECK_EQ(calc.base_max_hp(make_def({5, 5, 5, 5, 5, 5, 5})), 20);
    CHECK_EQ(calc.base_max_hp(make_def({5, 5, 3, 5, 5, 5, 5})), 12);
    CHECK_EQ(calc.base_max_hp(make_def({5, 5, 0, 5, 5, 5, 5})), 1);
    CHECK_EQ(calc.base_max_hp(make_def({5, 5, -2, 5, 5, 5, 5})), 1);
}

// MAX_MANA = CHA×3（最低 0）；起始法力 = CHA
TEST_CASE(stats_mana) {
    StatCalculator calc;
    CHECK_EQ(calc.base_max_mana(make_def({5, 5, 5, 5, 5, 5, 5})), 15);
    CHECK_EQ(calc.base_max_mana(make_def({5, 5, 5, 5, 8, 5, 5})), 24);
    CHECK_EQ(calc.base_starting_mana(make_def({5, 5, 5, 5, 8, 5, 5})), 8);
    CHECK_EQ(calc.base_max_mana(make_def({5, 5, 5, 5, -1, 5, 5})), 0);
}

// INITIATIVE = 2×SPD；MOVE_RANGE = max(1, 4 + trunc((SPD-5)/2))
TEST_CASE(stats_initiative_and_move) {
    StatCalculator calc;
    CHECK_EQ(calc.base_initiative(make_def({5, 5, 5, 5, 5, 5, 5})), 10);
    CHECK_EQ(calc.base_initiative(make_def({5, 5, 5, 5, 5, 8, 5})), 16);
    CHECK_EQ(calc.base_move_range(make_def({5, 5, 5, 5, 5, 5, 5})), 4);
    CHECK_EQ(calc.base_move_range(make_def({5, 5, 5, 5, 5, 7, 5})), 5);
    CHECK_EQ(calc.base_move_range(make_def({5, 5, 5, 5, 5, 9, 5})), 6);
    CHECK_EQ(calc.base_move_range(make_def({5, 5, 5, 5, 5, 3, 5})), 3);
    CHECK_EQ(calc.base_move_range(make_def({5, 5, 5, 5, 5, 1, 5})), 2);
    CHECK_EQ(calc.base_move_range(make_def({5, 5, 5, 5, 5, -1, 5})), 1);  // 最低 1
    // 移动预算 = 2×范围（DD-002 半步制）
    CHECK_EQ(calc.base_move_budget(make_def({5, 5, 5, 5, 5, 5, 5})), 8);
}

// CRIT = max(0, 2×LCK)%；敌人（can_crit=false）恒 0（DD）
TEST_CASE(stats_crit_chance) {
    StatCalculator calc;
    CHECK_EQ(calc.base_crit_chance_bp(make_def({5, 5, 5, 5, 5, 5, 5})), 1000);   // 10%
    CHECK_EQ(calc.base_crit_chance_bp(make_def({5, 5, 5, 5, 5, 5, 10})), 2000);  // 20%
    CHECK_EQ(calc.base_crit_chance_bp(make_def({5, 5, 5, 5, 5, 5, 0})), 0);
    CHECK_EQ(calc.base_crit_chance_bp(make_def({5, 5, 5, 5, 5, 5, 9}, false)), 0);  // 敌人不可暴击
}

// 回蓝 = max(INT, 0)
TEST_CASE(stats_mana_regen) {
    StatCalculator calc;
    CHECK_EQ(calc.base_mana_regen(make_def({5, 5, 5, 5, 5, 5, 5})), 5);
    CHECK_EQ(calc.base_mana_regen(make_def({5, 5, 5, 0, 5, 5, 5})), 0);
    CHECK_EQ(calc.base_mana_regen(make_def({5, 5, 5, -3, 5, 5, 5})), 0);
}

// 近战普攻 max(1, STR-5)；近战技能 max(1, floor((STR-5)/2))
TEST_CASE(stats_attack_formulas) {
    StatCalculator calc;
    CHECK_EQ(calc.base_melee_attack_damage(make_def({5, 5, 5, 5, 5, 5, 5})), 1);   // 最低 1
    CHECK_EQ(calc.base_melee_attack_damage(make_def({8, 5, 5, 5, 5, 5, 5})), 3);
    CHECK_EQ(calc.base_melee_attack_damage(make_def({3, 5, 5, 5, 5, 5, 5})), 1);
    CHECK_EQ(calc.base_melee_skill_damage(make_def({8, 5, 5, 5, 5, 5, 5})), 1);    // floor(3/2)=1
    CHECK_EQ(calc.base_melee_skill_damage(make_def({11, 5, 5, 5, 5, 5, 5})), 3);
    CHECK_EQ(calc.base_melee_skill_damage(make_def({2, 5, 5, 5, 5, 5, 5})), 1);    // 最低 1
}

// 战后回复：CON<5 → CON；否则 5 + trunc((CON-5)/2)
TEST_CASE(stats_post_battle_regen) {
    StatCalculator calc;
    CHECK_EQ(calc.base_post_battle_regen(make_def({5, 5, 5, 5, 5, 5, 5})), 5);
    CHECK_EQ(calc.base_post_battle_regen(make_def({5, 5, 7, 5, 5, 5, 5})), 6);
    CHECK_EQ(calc.base_post_battle_regen(make_def({5, 5, 3, 5, 5, 5, 5})), 3);
    CHECK_EQ(calc.base_post_battle_regen(make_def({5, 5, 1, 5, 5, 5, 5})), 1);
}
