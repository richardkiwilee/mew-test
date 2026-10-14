#pragma once
// ============================================================================
// PassiveDef / EffectDef / ConditionNode — 只读被动与效果定义（设计文档 §4.2/§4.3）
// 被动 = 静态属性修正 + 一组触发效果；效果 = 类型 + 触发 + 条件 + 目标 + 参数。
// 不做"万能效果 JSON"：每种 EffectType 使用固定参数集（本 struct 的子集）。
// ============================================================================

#include <string>
#include <vector>

#include "combat/core/basic_types.h"
#include "generated/reflection_gen.h"

namespace mew::combat {

// ---------------------------------------------------------------------------
// 条件树节点（受控条件，禁止任意脚本）
// ---------------------------------------------------------------------------
struct ConditionNode {
    ConditionOp op = ConditionOp::ALWAYS;
    int64_t value = 0;               // EVENT_AMOUNT_GE 阈值 / 比率（基点）/ 概率（基点）
    ResourceType resource = ResourceType::HP;  // EVENT_RESOURCE_IS
    StatType stat = StatType::STR;   // HIGHEST_STAT_UNIT 等
    uint32_t status_id = 0;          // HAS_STATUS 系列
    std::string text_param;          // EVENT_HAS_TAG / EVENT_LACKS_TAG 的标签名
    std::vector<ConditionNode> children;  // ALL / ANY / NOT

    static ConditionNode always() { return ConditionNode{}; }
    static ConditionNode all(std::vector<ConditionNode> cs) {
        ConditionNode n;
        n.op = ConditionOp::ALL;
        n.children = std::move(cs);
        return n;
    }
    static ConditionNode any(std::vector<ConditionNode> cs) {
        ConditionNode n;
        n.op = ConditionOp::ANY;
        n.children = std::move(cs);
        return n;
    }
    static ConditionNode make(ConditionOp o, int64_t v = 0, uint32_t status = 0,
                              ResourceType res = ResourceType::HP,
                              std::string text = {}) {
        ConditionNode n;
        n.op = o;
        n.value = v;
        n.status_id = status;
        n.resource = res;
        n.text_param = std::move(text);
        return n;
    }
};

// ---------------------------------------------------------------------------
// 静态属性修正（进入 StatCalculator 管线；Phase 4 生效）
// ---------------------------------------------------------------------------
struct StatModifierDef {
    StatType stat = StatType::STR;
    StatModifierOp operation = StatModifierOp::ADD;
    int32_t value = 0;               // MUL 时为基点（10000 = 100%）
    StackingMode stacking_mode = StackingMode::ADDITIVE;
    StatPhase phase = StatPhase::EQUIPMENT_AND_PASSIVE;
    uint32_t source_id = 0;          // 被动/装备定义 ID（UniqueBySource 用）
    uint32_t group_key = 0;          // 叠加分组（0 = 默认组）
    int32_t priority = 0;            // OVERRIDE 模式选谁生效（大者优先，平手按源序）
};

// ---------------------------------------------------------------------------
// 触发效果定义
// ---------------------------------------------------------------------------
struct EffectDef {
    uint32_t id = 0;                 // 所属定义内唯一
    EffectType type = EffectType::GAIN_RESOURCE;
    EventType trigger = EventType::BattleStarted;   // 订阅的事件
    EventPhase phase = EventPhase::AFTER;           // 反应窗口（排序第 1 键）
    int32_t priority = 0;            // 排序第 2 键（小者先）
    ConditionNode condition;         // 触发条件（ALWAYS = 恒真）
    TargetSelectorType target = TargetSelectorType::SELF;
    // 参数（按 type 使用固定子集）
    int64_t amount = 0;
    StatType stat = StatType::STR;
    DamageType damage_type = DamageType::PHYSICAL;  // DEAL_DAMAGE 用
    uint32_t unit_def_id = 0;        // SPAWN_UNIT 用
    ResourceType resource = ResourceType::HP;
    uint32_t status_id = 0;
    int32_t stacks = 1;
    int32_t duration = 0;            // 状态持续（按 StatusDef 的 duration_unit）
    int32_t radius = 0;              // UNITS_IN_RADIUS
    int32_t chance_bp = kPercentBase;  // 可选概率包装（<100% 时先掷骰）
    bool amount_scales_with_stacks = false;  // 状态 tick：数值 × 层数（Burn 每层 1 伤）
    std::string text_param;          // ADD_TAG/REMOVE_TAG 的标签名等
    // 触发次数限制
    TriggerLimitScope limit_scope = TriggerLimitScope::PER_EVENT;
    int32_t limit_count = 1;
};

// ---------------------------------------------------------------------------
// 被动定义
// ---------------------------------------------------------------------------
struct PassiveDef {
    uint32_t id = 0;
    std::string display_name;
    std::string description;
    std::vector<StatModifierDef> stat_modifiers;  // 静态修正（Phase 4 接入）
    std::vector<EffectDef> effects;               // 触发效果
};

}  // namespace mew::combat
