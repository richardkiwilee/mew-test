#pragma once
// ============================================================================
// TriggerDispatcher — 事件 → 订阅效果分发（设计文档 §5）
// 稳定排序键（§5.3）：phase_rank → priority(小者先) → 所有者顺序
// （当前行动单位最先，其余按 entity_id）→ 被动获得序 → 效果定义序 → instance_id。
// 每一步（匹配/条件/目标/限制/执行）都写日志，可完整追踪"为什么触发/没触发"。
// ============================================================================

#include "combat/domain/combat_event.h"
#include "combat/domain/passive_def.h"
#include "combat/domain/passive_runtime.h"
#include "combat/effects/condition_evaluator.h"
#include "combat/effects/effect_registry.h"
#include "combat/effects/target_selector.h"

namespace mew::combat {

class BattleContext;

class TriggerDispatcher {
public:
    TriggerDispatcher(BattleContext& ctx, EffectRegistry& registry);

    // 分发一个事件的全部订阅效果（含连锁：效果产生的新事件进入队列，由 EventQueue 驱动）
    void dispatch(const CombatEvent& event);

    // 触发计数窗口重置（由 BattleContext 在回合/轮/战斗边界调用）
    void on_turn_started();
    void on_round_started();
    void on_battle_started();

private:
    struct Match {
        PassiveRuntime* runtime = nullptr;
        const EffectDef* effect = nullptr;
        uint32_t effect_index = 0;
        uint32_t owner_entity = kInvalidEntity;
        int32_t phase_rank = 0;
    };

    void collect_matches(const CombatEvent& event, std::vector<Match>& out) const;
    void execute_match(const Match& match, const CombatEvent& event);

    BattleContext& ctx_;
    EffectRegistry& effects_;   // 共享注册表（BattleContext 持有）
    ConditionEvaluator conditions_;
    TargetSelector targets_;
};

}  // namespace mew::combat
