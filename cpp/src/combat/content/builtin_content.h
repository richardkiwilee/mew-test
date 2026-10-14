#pragma once
// ============================================================================
// 内置内容数据（Phase 2 初版：猫模板 + 2 种敌人；Phase 6/8 扩充）
// 数值来源：docs/mewgenics_research.md（wiki 明示公式）+ DD-017。
// ============================================================================

#include "combat/domain/unit_def.h"

namespace mew::combat {

class DefinitionCatalog;

// 单位定义 ID
namespace unit_ids {
constexpr uint32_t kCat = 1;
constexpr uint32_t kRat = 2;
constexpr uint32_t kBat = 3;
constexpr uint32_t kThug = 4;      // 高血近战
constexpr uint32_t kImp = 5;       // 远程火球法师
constexpr uint32_t kSpitter = 6;   // 远程毒液
}  // namespace unit_ids

// 技能定义 ID
namespace ability_ids {
constexpr uint32_t kFireball = 201;   // 3 蓝，远程魔法 5 伤 + 燃烧 1
constexpr uint32_t kHeal = 202;       // 2 蓝，友方治疗 4
constexpr uint32_t kZap = 203;        // 2 蓝，远程魔法 2 伤
constexpr uint32_t kPoisonSpit = 204; // 1 蓝，远程 1 伤 + 中毒 2
constexpr uint32_t kFlameSweep = 205; // 4 蓝，半径 1 溅射火焰（演示 AoE）
}  // namespace ability_ids

// 被动定义 ID
namespace passive_ids {
constexpr uint32_t kManaScholar = 101;   // 友军消耗 ≥7 法力 → 事件来源获得 all_stats_up
constexpr uint32_t kHunterReward = 102;  // 击杀敌人 → 自己 +3 法力 + strength_up
constexpr uint32_t kSwapExtremes = 103;  // 战斗开始时交换最高/最低属性（一次性，设计文档 §9.3）
constexpr uint32_t kSuperLuck = 104;     // 幸运 +21（装备/被动阶段常驻修正）
constexpr uint32_t kBrave = 105;         // STR +2
constexpr uint32_t kAgile = 106;         // +2 移动（= SPD +4，wiki: Agile 页）
constexpr uint32_t kRegen = 107;         // 回合结束回血 +1（演示）
constexpr uint32_t kThorns = 108;        // 受到伤害时反伤 2（TRUE）

// 装备（Phase 8：准备阶段配置，以被动形式挂到猫上；装备/被动属性阶段）
constexpr uint32_t kEqLuckyCharm = 110;  // 幸运护符：LCK +3
constexpr uint32_t kEqStrengthBelt = 111; // 力量腰带：STR +2
constexpr uint32_t kEqSwiftBoots = 112;   // 疾风之靴：SPD +2
constexpr uint32_t kEqScholarCap = 113;   // 学者之帽：INT +2
constexpr uint32_t kEqHeartAmulet = 114;  // 生命护符：CON +2
}  // namespace passive_ids

// 状态定义 ID
namespace status_ids {
constexpr uint32_t kAllStatsUp = 1;   // 全属性 +1
constexpr uint32_t kStrengthUp = 2;   // STR +2
constexpr uint32_t kBurn = 3;         // 回合开始每层 1 伤害，然后 −1 层
constexpr uint32_t kPoison = 4;       // 回合开始每层 1 伤害（不衰减，被治疗消耗——Phase 4 简化）
constexpr uint32_t kBleed = 5;        // 轮末受 =层数 伤害，然后 +1 层
constexpr uint32_t kStun = 6;         // 跳过回合（DD-019）
constexpr uint32_t kWeakness = 7;     // STR/DEX/SPD −2
constexpr uint32_t kWet = 8;          // 湿润（水域/苔沼；火焰移除）
}  // namespace status_ids

// 注册全部内置内容（单位 + 状态 + 被动 + 技能）
void register_builtin_content(DefinitionCatalog& catalog);

// 地形交互表（wiki: Tiles 页；Phase 6 接入移动结算）
int32_t terrain_enter_damage(TerrainType t);      // 进入地形受到的伤害（0 = 无）
bool terrain_applies_wet(TerrainType t);          // 进入是否施加 Wet

// 按 7 属性数组构造猫定义（demo 准备阶段随机属性用）
UnitDef make_cat_def(uint32_t id, const std::array<int32_t, kStatCount>& stats,
                     const std::string& name,
                     const std::vector<uint32_t>& passive_ids_list = {});

}  // namespace mew::combat
