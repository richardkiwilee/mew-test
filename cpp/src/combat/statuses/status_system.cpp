#include "pch.h"
#include "combat/statuses/status_system.h"

#include "combat/core/battle_context.h"

namespace mew::combat {

StatusSystem::StatusSystem(BattleContext& ctx) : ctx_(ctx) {}

StatusInstance* StatusSystem::find_instance(EntityId target, uint32_t status_def_id) {
    for (auto& inst : instances_) {
        if (inst.target_entity == target && inst.status_def_id == status_def_id) return &inst;
    }
    return nullptr;
}

const StatusInstance* StatusSystem::find_instance(EntityId target, uint32_t status_def_id) const {
    for (const auto& inst : instances_) {
        if (inst.target_entity == target && inst.status_def_id == status_def_id) return &inst;
    }
    return nullptr;
}

bool StatusSystem::apply(EntityId target, uint32_t status_def_id, int32_t stacks, EntityId source,
                         std::string_view reason) {
    UnitRuntime* u = ctx_.unit(target);
    const StatusDef* def = ctx_.catalog().status_def(status_def_id);
    if (!u || !def) {
        ctx_.log_.trace("Status", "StatusApplyRejected",
                        std::format("target={} def={} (unknown)", target, status_def_id));
        return false;
    }
    if (!u->is_alive()) {
        ctx_.log_.trace("Status", "StatusApplyRejected",
                        std::format("target={} not alive", target));
        return false;
    }
    if (stacks <= 0) stacks = 1;

    // 状态交互（设计文档 §10.4，显式标签规则）：
    // 火焰类状态施加时移除目标身上的湿润（wiki 元素交互：火蒸发水）
    if (std::find(def->tags.begin(), def->tags.end(), "fire") != def->tags.end()) {
        for (const auto& inst : instances_) {
            if (inst.target_entity != target) continue;
            const StatusDef* other = ctx_.catalog().status_def(inst.status_def_id);
            if (other && std::find(other->tags.begin(), other->tags.end(), "wet") != other->tags.end()) {
                remove(target, inst.status_def_id, "fire_interaction");
                break;
            }
        }
    }

    StatusInstance* existing = find_instance(target, status_def_id);
    const int32_t max_stacks = def->max_stacks;

    switch (def->stack_policy) {
        case StackPolicy::INDEPENDENT_INSTANCES: {
            existing = nullptr;  // 每次创建独立实例
            break;
        }
        case StackPolicy::STACK_AND_REFRESH: {
            if (existing) {
                existing->stacks = max_stacks > 0
                                       ? std::min(existing->stacks + stacks, max_stacks)
                                       : existing->stacks + stacks;
                existing->remaining_duration = def->duration_value;
                existing->source_entity = source;
                ctx_.log_.append("Status", "StatusStackChanged",
                                 {f_int("entity", target), f_int("status", status_def_id),
                                  f_int("stacks", existing->stacks), f_str("reason", std::string(reason))},
                                 std::format("entity={} status={} stacks={}", target, status_def_id,
                                             existing->stacks));
                return true;
            }
            break;
        }
        case StackPolicy::STACK_WITHOUT_REFRESH: {
            if (existing) {
                existing->stacks = max_stacks > 0
                                       ? std::min(existing->stacks + stacks, max_stacks)
                                       : existing->stacks + stacks;
                existing->source_entity = source;
                ctx_.log_.append("Status", "StatusStackChanged",
                                 {f_int("entity", target), f_int("status", status_def_id),
                                  f_int("stacks", existing->stacks)},
                                 std::format("entity={} status={} stacks={} (no refresh)", target,
                                             status_def_id, existing->stacks));
                return true;
            }
            break;
        }
        case StackPolicy::REFRESH_ONLY: {
            if (existing) {
                existing->remaining_duration = def->duration_value;
                ctx_.log_.append("Status", "StatusStackChanged",
                                 {f_int("entity", target), f_int("status", status_def_id),
                                  f_int("stacks", existing->stacks)},
                                 std::format("entity={} status={} duration refreshed", target,
                                             status_def_id));
                return true;
            }
            break;
        }
        case StackPolicy::REPLACE_STRONGER: {
            if (existing) {
                if (stacks > existing->stacks) {
                    existing->stacks = max_stacks > 0 ? std::min(stacks, max_stacks) : stacks;
                    existing->remaining_duration = def->duration_value;
                    existing->source_entity = source;
                }
                // 更弱/相等：不覆盖（保持原有持续时间）
                ctx_.log_.trace("Status", "StatusReplaceWeakerIgnored",
                                std::format("entity={} status={} existing={} incoming={}", target,
                                            status_def_id, existing->stacks, stacks));
                return true;
            }
            break;
        }
        case StackPolicy::UNIQUE: {
            if (existing) {
                ctx_.log_.trace("Status", "StatusUniqueIgnored",
                                std::format("entity={} status={} already present", target,
                                            status_def_id));
                return true;
            }
            break;
        }
    }

    // 创建新实例
    StatusInstance inst;
    inst.instance_id = ++instance_seq_;
    inst.status_def_id = status_def_id;
    inst.source_entity = source;
    inst.target_entity = target;
    inst.stacks = max_stacks > 0 ? std::min(stacks, max_stacks) : stacks;
    inst.remaining_duration = def->duration_value;
    inst.applied_seq = static_cast<uint32_t>(instances_.size());
    inst.applied_turn_seq = u->turn_seq;  // DD-008：施加当回合不计时长
    instances_.push_back(inst);
    ctx_.log_.append("Status", "StatusApplied",
                     {f_int("entity", target), f_int("status", status_def_id),
                      f_int("stacks", inst.stacks), f_int("duration", inst.remaining_duration),
                      f_int("source", source), f_str("reason", std::string(reason))},
                     std::format("entity={} status={} stacks={} dur={}", target, status_def_id,
                                 inst.stacks, inst.remaining_duration));
    // 属性可能变化（状态修正）→ 通知调度器重排未开始的回合
    ctx_.stats_.notify_stats_changed(ctx_);
    // IMMEDIATE：施加后立即执行一次 tick 效果（§10.4）
    if (def->tick_timing == TickTiming::IMMEDIATE) {
        run_tick(instances_.back().instance_id, TickTiming::IMMEDIATE);
    }
    return true;
}

int32_t StatusSystem::remove(EntityId target, uint32_t status_def_id, std::string_view reason) {
    int32_t removed = 0;
    for (auto& inst : instances_) {
        if (inst.target_entity == target && inst.status_def_id == status_def_id) {
            ++removed;
            ctx_.log_.append("Status", "StatusRemoved",
                             {f_int("entity", target), f_int("status", status_def_id),
                              f_str("reason", std::string(reason))},
                             std::format("entity={} status={} removed ({})", target, status_def_id,
                                         reason));
        }
    }
    std::erase_if(instances_, [&](const StatusInstance& inst) {
        return inst.target_entity == target && inst.status_def_id == status_def_id;
    });
    if (removed > 0) ctx_.stats_.notify_stats_changed(ctx_);
    return removed;
}

bool StatusSystem::remove_instance(EntityId target, InstanceId instance_id, std::string_view reason) {
    const auto it = std::find_if(instances_.begin(), instances_.end(),
                                 [&](const StatusInstance& inst) {
                                     return inst.target_entity == target
                                            && inst.instance_id == instance_id;
                                 });
    if (it == instances_.end()) return false;
    ctx_.log_.append("Status", "StatusRemoved",
                     {f_int("entity", target), f_int("status", it->status_def_id),
                      f_str("reason", std::string(reason))},
                     std::format("entity={} status={} instance removed ({})", target,
                                 it->status_def_id, reason));
    instances_.erase(it);
    return true;
}

bool StatusSystem::has(EntityId target, uint32_t status_def_id) const {
    return find_instance(target, status_def_id) != nullptr;
}

int32_t StatusSystem::stacks_of(EntityId target, uint32_t status_def_id) const {
    const StatusInstance* inst = find_instance(target, status_def_id);
    return inst ? inst->stacks : 0;
}

const std::vector<StatusInstance>& StatusSystem::instances_of(EntityId target) const {
    // 目标相关实例：就地收集（战斗规模小，线性扫描即可）
    static thread_local std::vector<StatusInstance> scratch;
    scratch.clear();
    for (const auto& inst : instances_) {
        if (inst.target_entity == target) scratch.push_back(inst);
    }
    return scratch;
}

// ---------------------------------------------------------------------------
// Phase 4：tick 推进（时机由回合流水线调用）
// ---------------------------------------------------------------------------
void StatusSystem::run_tick(InstanceId instance_id, TickTiming timing) {
    StatusInstance* inst = nullptr;
    for (auto& i : instances_) {
        if (i.instance_id == instance_id) {
            inst = &i;
            break;
        }
    }
    if (!inst) return;  // 已被移除（tick 效果杀死了持有者等）
    const StatusDef* def = ctx_.catalog().status_def(inst->status_def_id);
    if (!def) return;

    // 1. 发布 StatusTicked 事件（可追踪、可被被动订阅）
    ctx_.publish_event(
        CombatEvent{.type = EventType::StatusTicked, .phase = EventPhase::AFTER,
                    .source = inst->source_entity, .target = inst->target_entity,
                    .amount = inst->stacks},
        "Status",
        {f_int("status", inst->status_def_id), f_int("stacks", inst->stacks)},
        std::format("entity={} status={} ticks stacks={}", inst->target_entity,
                    inst->status_def_id, inst->stacks));

    // 2. 执行 tick 效果（先结算再衰减：tick_before_decay，DD-008）
    if (def->tick_before_decay) {
        for (const auto& eff : def->effects) {
            EffectDef scaled = eff;
            if (eff.amount_scales_with_stacks) scaled.amount *= inst->stacks;
            CombatEvent ev;
            ev.type = EventType::StatusTicked;
            ev.phase = EventPhase::AFTER;
            ev.source = inst->source_entity;
            ev.target = inst->target_entity;
            ev.amount = inst->stacks;
            ctx_.effects().execute(scaled.type, ctx_, scaled, ev, inst->source_entity,
                                   {inst->target_entity});
        }
    }

    // tick 效果可能杀死持有者（倒地清除状态）→ 重新定位
    inst = nullptr;
    for (auto& i : instances_) {
        if (i.instance_id == instance_id) {
            inst = &i;
            break;
        }
    }
    if (!inst) return;

    // 3. 层数变化（Burn 衰减 / Bleed 成长）
    if (def->decay_stacks_on_tick) --inst->stacks;
    inst->stacks += def->stack_delta_on_tick;
    if (inst->stacks <= 0 && def->expire_when_stacks_zero) {
        ctx_.log_.append("Status", "StatusExpired",
                         {f_int("entity", inst->target_entity), f_int("status", inst->status_def_id)},
                         std::format("entity={} status={} stacks reached 0",
                                     inst->target_entity, inst->status_def_id));
        std::erase_if(instances_, [&](const StatusInstance& i) { return i.instance_id == instance_id; });
        return;
    }

    // 4. 时长推进（按 tick 时机对应的计时单位；DD-008 施加回合不计）
    if (inst->remaining_duration > 0) {
        bool should_decrement = false;
        if (timing == TickTiming::END_OF_TURN && def->duration_unit == DurationUnit::ACTOR_TURNS) {
            const UnitRuntime* holder = ctx_.unit(inst->target_entity);
            should_decrement = holder && inst->applied_turn_seq != holder->turn_seq;
        } else if (timing == TickTiming::ROUND_END && def->duration_unit == DurationUnit::ROUNDS) {
            should_decrement = true;
        }
        if (should_decrement) {
            --inst->remaining_duration;
            if (inst->remaining_duration <= 0) {
                ctx_.log_.append("Status", "StatusExpired",
                                 {f_int("entity", inst->target_entity),
                                  f_int("status", inst->status_def_id)},
                                 std::format("entity={} status={} duration elapsed",
                                             inst->target_entity, inst->status_def_id));
                std::erase_if(instances_,
                              [&](const StatusInstance& i) { return i.instance_id == instance_id; });
            }
        }
    }
}

void StatusSystem::tick_start_of_turn(EntityId unit_id) {
    std::vector<InstanceId> ids;
    for (const auto& inst : instances_) {
        if (inst.target_entity != unit_id) continue;
        const StatusDef* def = ctx_.catalog().status_def(inst.status_def_id);
        if (def && def->tick_timing == TickTiming::START_OF_TURN) ids.push_back(inst.instance_id);
    }
    for (InstanceId id : ids) run_tick(id, TickTiming::START_OF_TURN);
}

void StatusSystem::tick_end_of_turn(EntityId unit_id) {
    std::vector<InstanceId> ids;
    for (const auto& inst : instances_) {
        if (inst.target_entity != unit_id) continue;
        const StatusDef* def = ctx_.catalog().status_def(inst.status_def_id);
        if (def && def->tick_timing == TickTiming::END_OF_TURN) ids.push_back(inst.instance_id);
    }
    for (InstanceId id : ids) run_tick(id, TickTiming::END_OF_TURN);
    // 无 tick 效果但需要时长推进的状态也走 run_tick（效果列表为空同样推进时长）
    ids.clear();
    for (const auto& inst : instances_) {
        if (inst.target_entity != unit_id) continue;
        const StatusDef* def = ctx_.catalog().status_def(inst.status_def_id);
        if (def && def->tick_timing != TickTiming::END_OF_TURN && inst.remaining_duration > 0) {
            ids.push_back(inst.instance_id);
        }
    }
    for (InstanceId id : ids) run_tick(id, TickTiming::END_OF_TURN);
}

void StatusSystem::tick_round_end() {
    std::vector<InstanceId> ids;
    for (const auto& inst : instances_) {
        const StatusDef* def = ctx_.catalog().status_def(inst.status_def_id);
        if (def && def->tick_timing == TickTiming::ROUND_END) ids.push_back(inst.instance_id);
    }
    for (InstanceId id : ids) run_tick(id, TickTiming::ROUND_END);
    // ROUNDS 计时单位的时长推进
    ids.clear();
    for (const auto& inst : instances_) {
        const StatusDef* def = ctx_.catalog().status_def(inst.status_def_id);
        if (def && def->tick_timing != TickTiming::ROUND_END && inst.remaining_duration > 0
            && def->duration_unit == DurationUnit::ROUNDS) {
            ids.push_back(inst.instance_id);
        }
    }
    for (InstanceId id : ids) run_tick(id, TickTiming::ROUND_END);
}

bool StatusSystem::blocked_by_status(EntityId unit_id) const {
    for (const auto& inst : instances_) {
        if (inst.target_entity != unit_id) continue;
        const StatusDef* def = ctx_.catalog().status_def(inst.status_def_id);
        if (def && def->blocks_turn) return true;
    }
    return false;
}

void StatusSystem::clear_on_down(EntityId target) {
    const auto keep = [&](const StatusInstance& inst) {
        if (inst.target_entity != target) return true;
        const StatusDef* def = ctx_.catalog().status_def(inst.status_def_id);
        return def == nullptr || !def->removable;
    };
    const size_t before = instances_.size();
    std::erase_if(instances_, [&](const StatusInstance& inst) { return !keep(inst); });
    if (instances_.size() != before) {
        ctx_.log_.append("Status", "StatusesClearedOnDown", {f_int("entity", target)},
                         std::format("entity={} statuses cleared on down", target));
        ctx_.stats_.notify_stats_changed(ctx_);
    }
}

}  // namespace mew::combat
