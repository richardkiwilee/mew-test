#include "pch.h"
#include "combat/effects/trigger_dispatcher.h"

#include "combat/core/battle_context.h"

namespace mew::combat {

namespace {

int32_t phase_rank(EventPhase phase) {
    switch (phase) {
        case EventPhase::BEFORE: return 0;
        case EventPhase::COMMIT: return 1;
        case EventPhase::AFTER:  return 2;
    }
    return 2;
}

}  // namespace

TriggerDispatcher::TriggerDispatcher(BattleContext& ctx, EffectRegistry& registry)
    : ctx_(ctx), effects_(registry), conditions_(ctx), targets_(ctx) {}

void TriggerDispatcher::collect_matches(const CombatEvent& event, std::vector<Match>& out) const {
    // 遍历单位（entity_id 升序 = 出生序）与被动（获得序）与效果（定义序）
    for (const auto& u : ctx_.units()) {
        if (!u.is_alive()) continue;  // 倒地/死亡单位被动不再触发
        for (const PassiveRuntime& rt : ctx_.passive_runtimes()) {
            if (rt.owner_entity != u.entity_id || !rt.enabled) continue;
            const PassiveDef* def = ctx_.catalog().passive_def(rt.passive_def_id);
            if (!def) continue;
            for (uint32_t i = 0; i < def->effects.size(); ++i) {
                const EffectDef& eff = def->effects[i];
                if (eff.trigger != event.type) continue;
                Match m;
                m.runtime = const_cast<PassiveRuntime*>(&rt);
                m.effect = &eff;
                m.effect_index = i;
                m.owner_entity = u.entity_id;
                m.phase_rank = phase_rank(eff.phase);
                out.push_back(m);
            }
        }
    }

    // 稳定排序（设计文档 §5.3）
    std::stable_sort(out.begin(), out.end(), [](const Match& a, const Match& b) {
        if (a.phase_rank != b.phase_rank) return a.phase_rank < b.phase_rank;
        if (a.effect->priority != b.effect->priority) return a.effect->priority < b.effect->priority;
        if (a.owner_entity != b.owner_entity) return a.owner_entity < b.owner_entity;
        if (a.runtime->acquisition_seq != b.runtime->acquisition_seq)
            return a.runtime->acquisition_seq < b.runtime->acquisition_seq;
        if (a.effect_index != b.effect_index) return a.effect_index < b.effect_index;
        return a.runtime->instance_id < b.runtime->instance_id;
    });
}

void TriggerDispatcher::execute_match(const Match& match, const CombatEvent& event) {
    const EffectDef& eff = *match.effect;
    PassiveRuntime& rt = *match.runtime;
    const std::string who = std::format("owner={} passive={} effect={}", match.owner_entity,
                                        rt.passive_def_id, eff.id);

    // 1. 同一事件内同一效果实例防重（DD-009 幂等）
    if (rt.last_event_id == event.event_id && rt.last_effect_index == static_cast<int32_t>(match.effect_index)) {
        ctx_.log_.trace("Trigger", "TriggerOnceGuard",
                        std::format("{} evt={} already fired", who, event.event_id));
        return;
    }

    // 2. 触发次数限制（每个效果独立计数，PER_EVENT 窗口在 dispatch 时已重置）
    const size_t scope_idx = static_cast<size_t>(eff.limit_scope) - 1;
    if (match.effect_index >= rt.effect_counters.size()) {
        ctx_.log_.trace("Trigger", "TriggerCounterMissing",
                        std::format("{} effect={} (runtime counters not sized)", who, eff.id));
        return;
    }
    auto& counters = rt.effect_counters[match.effect_index];
    if (counters[scope_idx] >= eff.limit_count) {
        ctx_.log_.trace("Trigger", "TriggerLimitReached",
                        std::format("{} scope={} limit={}", who, to_string(eff.limit_scope),
                                    eff.limit_count));
        return;
    }

    ctx_.log_.trace("Trigger", "TriggerMatched",
                    std::format("{} trigger={} evt={}", who, to_string(eff.trigger), event.event_id));

    // 3. 条件判定（记录失败原因）
    {
        EvalContext ec{event, match.owner_entity};
        std::string why;
        if (!conditions_.evaluate(eff.condition, ec, &why)) {
            ctx_.log_.trace("Trigger", "ConditionFailed",
                            std::format("{} : {}", who, why.empty() ? "?" : why));
            return;
        }
        ctx_.log_.trace("Trigger", "ConditionPassed", who);
    }

    // 4. 目标解析
    TargetContext tc{event, match.owner_entity};
    const std::vector<EntityId> targets = targets_.resolve(eff, tc);
    if (targets.empty()) {
        ctx_.log_.trace("Trigger", "NoTargetsFound",
                        std::format("{} selector={}", who, to_string(eff.target)));
        return;
    }

    // 5. 概率包装（条件通过、目标确定之后掷骰，§13）
    if (eff.chance_bp < kPercentBase) {
        if (!ctx_.rng().roll_chance(eff.chance_bp, "effect_chance")) {
            ctx_.log_.trace("Trigger", "ChanceRollFailed",
                            std::format("{} chance={}bp", who, eff.chance_bp));
            return;
        }
    }

    // 6. 计数与防重标记（先标记后执行：执行中产生的事件不会再触发本效果）
    ++counters[scope_idx];
    rt.last_event_id = event.event_id;
    rt.last_effect_index = static_cast<int32_t>(match.effect_index);

    // 7. 执行
    ctx_.log_.trace("Effect", "EffectExecuted",
                    std::format("{} type={} targets={}", who, to_string(eff.type), targets.size()));
    effects_.execute(eff.type, ctx_, eff, event, match.owner_entity, targets);
}

void TriggerDispatcher::dispatch(const CombatEvent& event) {
    // 计数窗口推进：新事件 → 重置各效果 PER_EVENT 计数；新行动 → 重置 PER_ACTION
    const size_t per_event = static_cast<size_t>(TriggerLimitScope::PER_EVENT) - 1;
    const size_t per_action = static_cast<size_t>(TriggerLimitScope::PER_ACTION) - 1;
    for (auto& rt : ctx_.passive_runtimes()) {
        if (rt.counter_event_id != event.event_id) {
            rt.counter_event_id = event.event_id;
            for (auto& counters : rt.effect_counters) counters[per_event] = 0;
        }
        if (event.type == EventType::ActionStarted && rt.counter_action_id != event.action_id) {
            rt.counter_action_id = event.action_id;
            for (auto& counters : rt.effect_counters) counters[per_action] = 0;
        }
    }
    std::vector<Match> matches;
    collect_matches(event, matches);
    for (const Match& match : matches) {
        execute_match(match, event);
    }
}

void TriggerDispatcher::on_turn_started() {
    const size_t per_turn = static_cast<size_t>(TriggerLimitScope::PER_TURN) - 1;
    for (auto& rt : ctx_.passive_runtimes()) {
        for (auto& counters : rt.effect_counters) counters[per_turn] = 0;
    }
}

void TriggerDispatcher::on_round_started() {
    const size_t per_round = static_cast<size_t>(TriggerLimitScope::PER_ROUND) - 1;
    for (auto& rt : ctx_.passive_runtimes()) {
        for (auto& counters : rt.effect_counters) counters[per_round] = 0;
    }
}

void TriggerDispatcher::on_battle_started() {
    for (auto& rt : ctx_.passive_runtimes()) {
        for (auto& counters : rt.effect_counters) counters.fill(0);
    }
}

}  // namespace mew::combat
