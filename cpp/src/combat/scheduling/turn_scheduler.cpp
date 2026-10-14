#include "pch.h"
#include "combat/scheduling/turn_scheduler.h"

#include "combat/core/battle_context.h"

namespace mew::combat {

void TurnScheduler::start_battle() {
    // DD-003：tie-break 战斗开始时随机一次，整场不变（wiki: "that RNG persisting throughout"）
    for (auto& u : ctx_.units_) {
        u.initiative_tie_break = ctx_.rng_.next_tie_break("initiative_tie_break");
    }
    round_ = 1;
    build_round_queue();
}

void TurnScheduler::build_round_queue() {
    queue_.clear();
    queue_seq_ = 0;

    // 新轮次：重置回合内标记（只对仍存活的单位）
    for (auto& u : ctx_.units_) {
        u.acted_this_round = false;
        u.extra_turns_this_round = 0;
    }

    // 候选：存活单位（倒地/死亡/移除不行动）
    std::vector<UnitRuntime*> candidates;
    for (auto& u : ctx_.units_) {
        if (u.is_alive()) candidates.push_back(&u);
    }

    // 稳定排序：先攻降序 → tie-break 升序 → 出生序升序（DD-003 / 设计文档 §5.3）
    std::stable_sort(candidates.begin(), candidates.end(), [&](const UnitRuntime* a, const UnitRuntime* b) {
        const int32_t ia = ctx_.stats_.initiative(ctx_, a->entity_id);
        const int32_t ib = ctx_.stats_.initiative(ctx_, b->entity_id);
        if (ia != ib) return ia > ib;
        if (a->initiative_tie_break != b->initiative_tie_break)
            return a->initiative_tie_break < b->initiative_tie_break;
        return a->spawn_seq < b->spawn_seq;
    });

    for (const UnitRuntime* u : candidates) {
        TurnEntry e;
        e.entity = u->entity_id;
        e.kind = TurnKind::MAIN;
        e.seq = ++queue_seq_;
        queue_.push_back(e);
    }

    // 行动顺序日志（调试：确认先攻排序符合预期）
    {
        std::vector<LogField> fields;
        std::string detail;
        for (const UnitRuntime* u : candidates) {
            fields.push_back(f_int("entity", u->entity_id));
            fields.push_back(f_int("initiative", ctx_.stats_.initiative(ctx_, u->entity_id)));
            fields.push_back(f_int("tie_break", u->initiative_tie_break));
            if (!detail.empty()) detail += " ";
            detail += std::format("{}:init={}", u->entity_id,
                                  ctx_.stats_.initiative(ctx_, u->entity_id));
        }
        ctx_.log_.append("Scheduler", "TurnOrder", std::move(fields), std::move(detail));
    }

    if (!queue_.empty()) {
        ctx_.events_.on_round_started();
        ctx_.publish_event(
            CombatEvent{.type = EventType::RoundStarted, .phase = EventPhase::AFTER,
                        .amount = static_cast<int64_t>(round_)},
            "Scheduler", {}, std::format("round={}", round_));
    }
}

EntityId TurnScheduler::begin_next_turn() {
    if (stopped_) return kInvalidEntity;
    while (queue_.empty()) {
        if (stopped_) return kInvalidEntity;
        round_end();
        if (stopped_) return kInvalidEntity;
    }
    while (!queue_.empty()) {
        TurnEntry entry = queue_.front();
        queue_.pop_front();
        const UnitRuntime* u = ctx_.unit(entry.entity);
        if (!u || !u->is_alive()) {
            ctx_.log_.append("Scheduler", "TurnSkipped",
                             {f_int("entity", entry.entity)},
                             std::format("entity={} (not alive)", entry.entity));
            continue;
        }
        if (entry.kind == TurnKind::MAIN) {
            UnitRuntime* mu = ctx_.unit(entry.entity);
            mu->acted_this_round = true;
        }
        last_kind_ = entry.kind;
        ctx_.log_.append("Scheduler", "TurnBegins",
                         {f_int("entity", entry.entity), f_str("kind", std::string(to_string(entry.kind)))},
                         std::format("entity={} kind={}", entry.entity, to_string(entry.kind)));
        return entry.entity;
    }
    return kInvalidEntity;
}

void TurnScheduler::end_current_turn() {
    // 主回合条目已弹出；此处为 Phase 4 状态推进/队列重排预留
}

bool TurnScheduler::grant_extra_turn(EntityId entity, std::string_view reason) {
    // DD-004：额外回合插入当前单位之后（队首 = 下一个行动），每单位每轮上限 1
    UnitRuntime* u = ctx_.unit(entity);
    if (!u || !u->is_alive()) return false;
    if (u->extra_turns_this_round >= 1) {
        ctx_.log_.append("Scheduler", "ExtraTurnRejected",
                         {f_int("entity", entity), f_str("reason", std::string(reason))},
                         std::format("entity={} limit_reached", entity));
        return false;
    }
    ++u->extra_turns_this_round;
    TurnEntry e;
    e.entity = entity;
    e.kind = TurnKind::EXTRA;
    e.seq = ++queue_seq_;
    queue_.push_front(e);
    ctx_.log_.append("Scheduler", "ExtraTurnQueued",
                     {f_int("entity", entity), f_str("reason", std::string(reason))},
                     std::format("entity={} reason={}", entity, reason));
    return true;
}

void TurnScheduler::remove_entity(EntityId entity) {
    std::erase_if(queue_, [entity](const TurnEntry& e) { return e.entity == entity; });
}

void TurnScheduler::round_end() {
    // 轮末状态 tick（Bleed 等）先于 RoundEnded 事件（DD-011）
    ctx_.status_system_.tick_round_end();
    ctx_.publish_event(
        CombatEvent{.type = EventType::RoundEnded, .phase = EventPhase::AFTER,
                    .amount = static_cast<int64_t>(round_)},
        "Scheduler", {}, std::format("round={}", round_));
    ++round_;
    build_round_queue();
    if (queue_.empty()) {
        // 没有任何单位可以行动：战斗应已由 check_battle_end 结束
        ctx_.log_.append("Scheduler", "NoUnitsCanAct", {}, "queue empty");
    }
}

void TurnScheduler::rebuild_pending_order() {
    // Phase 4：速度/先攻变化 → 对未开始的项重排（保持已开始回合稳定，DD-014 除外）
    // tie-break 值不变，只按当前先攻重排
    std::vector<TurnEntry> pending(queue_.begin(), queue_.end());
    std::stable_sort(pending.begin(), pending.end(), [&](const TurnEntry& a, const TurnEntry& b) {
        const UnitRuntime* ua = ctx_.unit(a.entity);
        const UnitRuntime* ub = ctx_.unit(b.entity);
        if (!ua || !ub) return false;
        const int32_t ia = ctx_.stats_.initiative(ctx_, a.entity);
        const int32_t ib = ctx_.stats_.initiative(ctx_, b.entity);
        if (ia != ib) return ia > ib;
        if (ua->initiative_tie_break != ub->initiative_tie_break)
            return ua->initiative_tie_break < ub->initiative_tie_break;
        return a.seq < b.seq;
    });
    queue_.assign(pending.begin(), pending.end());
}

}  // namespace mew::combat
