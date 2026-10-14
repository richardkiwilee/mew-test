#pragma once
// ============================================================================
// PassiveRuntime — 被动运行时实例（设计文档 §4.2）
// 共享 PassiveDef，实例各自持有计数与状态；custom 数据只存"触发次数"这类
// 无法从世界状态推导的信息。
// ============================================================================

#include <array>

#include "combat/core/basic_types.h"

namespace mew::combat {

struct PassiveRuntime {
    InstanceId instance_id = 0;      // 全局唯一
    EntityId owner_entity = kInvalidEntity;
    uint32_t passive_def_id = 0;
    int32_t level = 1;
    uint32_t acquisition_seq = 0;    // 获得顺序（排序第 4 键）
    bool enabled = true;

    // 触发计数：每个效果独立计数（同一被动的多个效果互不挤占限制额度）
    // 外层索引 = 效果定义下标；内层按 TriggerLimitScope 索引
    std::vector<std::array<int32_t, enum_count(TriggerLimitScope{})>> effect_counters;

    // 计数窗口：PER_EVENT / PER_ACTION 计数属于哪个事件/行动
    EventId counter_event_id = kInvalidEvent;
    ActionId counter_action_id = kInvalidAction;

    // 防重：同一事件内同一效果实例只触发一次（DD-009 幂等）
    EventId last_event_id = kInvalidEvent;
    int32_t last_effect_index = -1;
};

}  // namespace mew::combat
