#pragma once
// ============================================================================
// TurnScheduler — 轮次与单位回合调度（设计文档 §6 / wiki: Battle/Speed）
// ============================================================================

#include <deque>
#include <string>
#include <vector>

#include "combat/core/basic_types.h"
#include "generated/reflection_gen.h"
// ============================================================================
// - 每单位每轮至少一个主回合；顺序 = Initiative 降序；
//   同先攻用战斗开始时生成的稳定 tie-break（整场不变，DD-003）。
// - 额外回合：插入当前单位之后（DD-004）；每单位每轮最多 1 个（防无限）。
// - 死亡/移除单位跳过其回合（日志 TurnSkipped）。
// - 回合内速度变化 → 对"尚未执行"的队列项重排（Phase 4 接入属性修正后生效）。
// ============================================================================

namespace mew::combat {

class BattleContext;

struct TurnEntry {
    EntityId entity = kInvalidEntity;
    TurnKind kind = TurnKind::MAIN;
    SequenceId seq = 0;  // 入队顺序（稳定 tie-break 兜底）
};

class TurnScheduler {
public:
    explicit TurnScheduler(BattleContext& ctx) : ctx_(ctx) {}

    // 战斗开始：为所有单位生成 tie-break（随机一次、固化），建立第 1 轮队列
    void start_battle();

    RoundIndex round_index() const { return round_; }
    TurnKind last_turn_kind() const { return last_kind_; }

    const TurnEntry* current() const;
    bool queue_empty() const { return queue_.empty(); }
    size_t queue_size() const { return queue_.size(); }

    // 弹出下一个可行动单位（跳过死亡/移除，记录 TurnSkipped）；
    // 队列耗尽时自动 round_end() 并重建下一轮队列。
    // 返回 kInvalidEntity 表示战斗已结束或无可行动单位。
    EntityId begin_next_turn();

    // 当前单位回合结束
    void end_current_turn();

    // 额外回合：插入当前单位之后（DD-004）；超限返回 false
    bool grant_extra_turn(EntityId entity, std::string_view reason);

    // 单位死亡/移除后从队列剔除
    void remove_entity(EntityId entity);

    // 战斗结束标记（停止调度）
    void stop() { stopped_ = true; }

    // 速度/先攻变化 → 对未开始的项按当前先攻重排（tie-break 不变，DD-014 除外）
    void rebuild_pending_order();

private:
    void build_round_queue();
    void round_end();

    BattleContext& ctx_;
    RoundIndex round_ = 1;
    std::deque<TurnEntry> queue_;
    SequenceId queue_seq_ = 0;
    TurnKind last_kind_ = TurnKind::MAIN;
    bool stopped_ = false;
};

}  // namespace mew::combat
