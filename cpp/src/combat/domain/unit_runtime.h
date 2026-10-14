#pragma once
// ============================================================================
// UnitRuntime — 单次战斗中的单位运行时状态（设计文档 §4.1）
// entity_id 战斗内唯一且稳定（= 容器下标 + 1），死亡/移除不回收；
// 定义数据只读，此处只存运行期状态。
// ============================================================================

#include <array>
#include <string>
#include <vector>

#include "combat/core/basic_types.h"
#include "combat/core/vec2i.h"
#include "generated/reflection_gen.h"

namespace mew::combat {

struct UnitRuntime {
    EntityId entity_id = kInvalidEntity;
    uint32_t def_id = 0;
    Team team = Team::NEUTRAL;
    Vec2i pos{-1, -1};                     // 不在棋盘上时 = (-1,-1)
    int32_t hp = 0;
    int32_t mana = 0;
    LifeState life_state = LifeState::ALIVE;
    uint32_t spawn_seq = 0;                // 确定性 tie-break 的兜底键（设计文档 §4.1）
    uint32_t initiative_tie_break = 0;     // 战斗开始时随机一次、整场不变（DD-003）
    int32_t corpse_health_remaining = 0;   // 尸体韧性余量（DD-007）

    // 每回合动作槽的 action point（0/1+，Refresh 语义见 DD-012）
    std::array<int32_t, enum_count(ActionSlot{})> action_points{};

    // 回合内标记
    bool acted_this_round = false;         // 本轮是否已获得主回合
    uint32_t extra_turns_this_round = 0;   // 本轮已获额外回合数（DD-004 防无限）
    uint32_t turn_seq = 0;                 // 已开始的单位回合计数（状态计时基准，DD-008）

    // 标签（ADD_TAG/REMOVE_TAG 效果；被动条件可用）
    std::vector<std::string> tags;

    // 属性：初始化快照经一次性变换后的"有效基底"（设计文档 §9.3/§9.4 —— 只写一次，
    // 每次重算从它出发叠加动态修正，绝不重复执行变换）
    std::array<int32_t, kStatCount> effective_base{};
    bool stats_initialized = false;

    bool is_alive() const { return life_state == LifeState::ALIVE; }
    bool is_down() const { return life_state == LifeState::DOWNED; }
    bool is_gone() const {
        return life_state == LifeState::DEAD || life_state == LifeState::REMOVED;
    }
};

}  // namespace mew::combat
