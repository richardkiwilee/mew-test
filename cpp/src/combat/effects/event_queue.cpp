#include "pch.h"
#include "combat/effects/event_queue.h"

#include "combat/core/battle_context.h"

namespace mew::combat {

EventQueue::EventQueue(BattleContext& ctx) : ctx_(ctx), dispatcher_(ctx, ctx.effects()) {}

void EventQueue::publish(CombatEvent event) {
    // 序号与事件 ID 由 BattleContext::publish_event 统一分配；直接调用时兜底分配
    if (event.seq == 0) event.seq = ctx_.next_event_seq();
    if (event.event_id == kInvalidEvent) event.event_id = ctx_.next_event_id();
    if (draining_) {
        // 连锁事件：继承当前处理事件的深度 +1（DD-009）
        event.depth = current_depth_ + 1;
    }
    queue_.push_back(std::move(event));
    if (!draining_) {
        drain();
    }
}

void EventQueue::drain() {
    draining_ = true;
    chain_events_ = 0;
    while (!queue_.empty()) {
        CombatEvent event = queue_.front();
        queue_.pop_front();
        current_depth_ = event.depth;

        // 因果链防护（DD-009）：超限终止并输出诊断日志
        if (event.depth > kMaxEventDepth) {
            ctx_.log_.append("Event", "ChainLimitReached",
                             {f_str("limit", "depth"), f_int("depth", event.depth),
                              f_int("evt", event.event_id)},
                             std::format("event={} depth={} exceeds {}", to_string(event.type),
                                         event.depth, kMaxEventDepth));
            continue;
        }
        if (++chain_events_ > kMaxChainEvents) {
            ctx_.log_.append("Event", "ChainLimitReached",
                             {f_str("limit", "events"), f_int("evt", event.event_id)},
                             std::format("event={} exceeds {} events per chain",
                                         to_string(event.type), kMaxChainEvents));
            continue;
        }

        dispatcher_.dispatch(event);
    }
    draining_ = false;
    current_depth_ = 0;
}

}  // namespace mew::combat
