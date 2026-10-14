#pragma once
// ============================================================================
// EventQueue — 有序事件队列与连锁反应保护（设计文档 §12.4 / DD-009）
// - 效果产生的新事件追加到队尾，当前窗口排空后才处理（工作队列，无同步递归）；
// - 因果链深度上限 kMaxEventDepth = 16；单条反应链事件上限 kMaxChainEvents = 64；
//   超限输出 ChainLimitReached 错误日志并丢弃，不静默卡死。
// ============================================================================

#include <deque>

#include "combat/domain/combat_event.h"
#include "combat/effects/trigger_dispatcher.h"

namespace mew::combat {

class BattleContext;

// DD-009 上限
constexpr int32_t kMaxEventDepth = 16;
constexpr uint32_t kMaxChainEvents = 64;

class EventQueue {
public:
    explicit EventQueue(BattleContext& ctx);

    // 发布事件（分配 seq/event_id/depth）并排空反应队列
    void publish(CombatEvent event);

    size_t pending() const { return queue_.size(); }

    // 触发计数窗口重置（回合/轮/战斗边界，转发给 dispatcher）
    void on_turn_started() { dispatcher_.on_turn_started(); }
    void on_round_started() { dispatcher_.on_round_started(); }
    void on_battle_started() { dispatcher_.on_battle_started(); }

private:
    void drain();

    BattleContext& ctx_;
    TriggerDispatcher dispatcher_;
    std::deque<CombatEvent> queue_;
    bool draining_ = false;
    int32_t current_depth_ = 0;        // 正在处理的事件深度
    uint32_t chain_events_ = 0;        // 本链已处理事件数
};

}  // namespace mew::combat
