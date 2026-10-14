#pragma once
// ============================================================================
// StatCalculator — 属性与派生属性计算（设计文档 §9）
// 计算顺序（§9.2，DD-018）：
//   BaseStats → PermanentProgression → EquipmentAndPassive → PreTransformTemporary
//   →（战斗初始化时一次性）OrderedStatTransforms（极值交换，只执行一次）
//   → 写入 UnitRuntime.effective_base（snapshot 已烘焙初始修正）
//   → 之后每次重算：effective_base + 初始化后新施加的动态修正（状态/被动）
//   → Clamp → DerivedStats
// 一次性变换绝不在普通重算中重复执行（设计文档 §0 规则 8 / §9.4）。
// ============================================================================

#include <array>

#include "combat/core/basic_types.h"
#include "combat/domain/unit_def.h"
#include "generated/reflection_gen.h"

namespace mew::combat {

class BattleContext;

class StatCalculator {
public:
    // ---------------- 基础公式（无修正，wiki 明示） ----------------
    int32_t base_stat(const UnitDef& def, StatType s) const { return def.base_stat(s); }

    // ---------------- 纯公式（按定义，无修正；wiki 公式单测用） ----------------
    int32_t base_max_hp(const UnitDef& def) const { return std::max(1, def.base_stat(StatType::CON) * 4); }
    int32_t base_max_mana(const UnitDef& def) const { return std::max(0, def.base_stat(StatType::CHA) * 3); }
    int32_t base_starting_mana(const UnitDef& def) const { return def.base_stat(StatType::CHA); }
    int32_t base_initiative(const UnitDef& def) const { return 2 * def.base_stat(StatType::SPD); }
    int32_t base_move_range(const UnitDef& def) const {
        return std::max(1, def.base_move + static_cast<int32_t>(div_trunc(def.base_stat(StatType::SPD) - 5, 2)));
    }
    int32_t base_move_budget(const UnitDef& def) const { return base_move_range(def) * 2; }
    int32_t base_crit_chance_bp(const UnitDef& def) const {
        if (!def.can_crit) return 0;
        return std::max(0, 2 * def.base_stat(StatType::LCK) * 100);
    }
    int32_t base_mana_regen(const UnitDef& def) const { return std::max(0, def.base_stat(StatType::INT)); }
    int32_t base_melee_attack_damage(const UnitDef& def) const {
        return std::max(1, def.base_stat(StatType::STR) - 5);
    }
    int32_t base_melee_skill_damage(const UnitDef& def) const {
        return std::max(1, static_cast<int32_t>(div_floor(def.base_stat(StatType::STR) - 5, 2)));
    }
    int32_t base_post_battle_regen(const UnitDef& def) const {
        const int32_t con = def.base_stat(StatType::CON);
        if (con < 5) return con;
        return 5 + static_cast<int32_t>(div_trunc(con - 5, 2));
    }

    // ---------------- 战斗内有效值（修正管线） ----------------
    // 主属性有效值：effective_base + 初始化后动态修正，逐阶段应用
    int32_t effective_stat(const BattleContext& ctx, EntityId id, StatType s) const;

    // 派生属性（基于有效主属性）
    int32_t derived(const BattleContext& ctx, EntityId id, DerivedStatType d) const;
    int32_t max_hp(const BattleContext& ctx, EntityId id) const;
    int32_t max_mana(const BattleContext& ctx, EntityId id) const;
    int32_t initiative(const BattleContext& ctx, EntityId id) const;
    int32_t move_range(const BattleContext& ctx, EntityId id) const;
    int32_t move_budget(const BattleContext& ctx, EntityId id) const;   // DD-002 半步制
    int32_t crit_chance_bp(const BattleContext& ctx, EntityId id) const;
    int32_t mana_regen(const BattleContext& ctx, EntityId id) const;
    int32_t health_regen(const BattleContext& ctx, EntityId id) const;
    int32_t melee_attack_damage(const BattleContext& ctx, EntityId id) const;
    int32_t melee_skill_damage(const BattleContext& ctx, EntityId id) const;
    int32_t post_battle_regen(const BattleContext& ctx, EntityId id) const;

    // ---------------- 战斗初始化 ----------------
    // 快照（base + 当前存在的修正）→ 写入 effective_base；
    // run_transforms=true 时执行被动声明的一次性变换（只在战斗初始化阶段传 true）
    void initialize_stats(BattleContext& ctx, EntityId id, bool run_transforms);

    // 起始法力 = 有效 CHA（wiki）
    int32_t starting_mana(const BattleContext& ctx, EntityId id) const {
        return effective_stat(ctx, id, StatType::CHA);
    }

    // 极值交换（设计文档 §9.3）：最大/最小两个属性槽位的值互换；
    // tie-break = 固定属性枚举顺序（同值时取枚举序更小者）；只对快照执行一次
    void swap_extreme_stats(std::array<int32_t, kStatCount>& values) const;

    // 属性变化后通知调度器重排未开始的回合（统一入口）
    void notify_stats_changed(BattleContext& ctx) const;

private:
    // 收集并应用某实体的动态修正（初始化后才施加的被动/状态实例）
    void apply_dynamic_modifiers(const BattleContext& ctx, EntityId id,
                                 std::array<int32_t, kStatCount>& values) const;
};

}  // namespace mew::combat
