#pragma once
// ============================================================================
// UnitDef — 只读单位定义（设计文档 §4.1 / §7）
// 定义数据只读，运行期状态全部在 UnitRuntime；内容数据见 content/。
// ============================================================================

#include <array>
#include <string>
#include <vector>

#include "combat/core/basic_types.h"
#include "generated/reflection_gen.h"

namespace mew::combat {

// 主属性数量（与 schema.py StatType 对齐）
constexpr size_t kStatCount = 7;

struct UnitDef {
    uint32_t id = 0;                       // 内容 ID（全局唯一）
    std::string display_name;              // 展示名（UI 用）
    std::array<int32_t, kStatCount> base_stats{};  // 按 StatType 枚举序：STR/DEX/CON/INT/CHA/SPD/LCK
    Team default_team = Team::NEUTRAL;
    int32_t base_move = 4;                 // 移动范围基数（猫 = 4）
    bool is_required_enemy = false;        // 胜负条件：是否"必需击杀"的敌人（DD-007）
    int32_t corpse_health = 0;             // 尸体韧性：玩家猫 3，敌人 0（DD-007）
    bool can_crit = false;                 // 能否随机暴击（DD：仅玩家猫等少数单位）
    std::vector<uint32_t> passive_ids;     // 初始被动（Phase 3 起生效）
    std::vector<uint32_t> ability_ids;     // 技能列表（Phase 6 起生效）
    uint32_t basic_attack_id = 0;          // 基础攻击定义（0 = 默认近战 Str-5 公式）

    int32_t base_stat(StatType s) const {
        return base_stats[static_cast<uint32_t>(s) - 1];
    }
};

}  // namespace mew::combat
