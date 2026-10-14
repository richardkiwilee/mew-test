#pragma once
// ============================================================================
// CombatLog — 结构化战斗结算日志（设计文档 §20）
// ============================================================================

#include <string>
#include <variant>
#include <vector>

#include "combat/core/basic_types.h"
// ============================================================================
// 目的：不是只显示文字，而是用于确认规则符合预期、复现与调试。
// 每条记录包含稳定的 message_key + 结构化字段；中文文案仅出现在渲染层。
// 上下文（round/current_actor/action）由战斗流程设置，append 时快照进条目。
// ============================================================================

namespace mew::combat {

// 结构化日志字段值
using LogValue = std::variant<int64_t, std::string, bool>;

struct LogField {
    std::string key;      // 如 "entity" / "amount" / "status"
    LogValue value;
};

// 日志级别：Trace = 调试追踪（为什么触发/为什么没触发）；Info = 正常结算事件
enum class LogLevel : uint8_t { Trace = 0, Info = 1 };

struct LogEntry {
    SequenceId sequence = 0;         // 1 起始全局顺序
    LogLevel level = LogLevel::Info;
    RoundIndex round_index = 0;      // 0 = 轮次外（战斗开始/结束）
    EntityId current_actor = kInvalidEntity;
    ActionId action_id = kInvalidAction;
    EventId event_id = kInvalidEvent;
    EventId parent_event_id = kInvalidEvent;
    EntityId source_entity = kInvalidEntity;
    EntityId target_entity = kInvalidEntity;
    std::string category;            // "Scheduler" / "Damage" / "Trigger" / ...
    std::string message_key;         // 稳定 key，如 "TurnStarted"；测试依赖它
    std::vector<LogField> fields;    // 结构化 payload
    std::string detail;              // 渲染好的可读文本（可为空，由 render 兜底）
};

class CombatLog {
public:
    // 设置后续 append 的上下文快照
    void set_round(RoundIndex round) { ctx_.round_index = round; }
    void set_current_actor(EntityId actor) { ctx_.current_actor = actor; }
    void set_action(ActionId action) { ctx_.action_id = action; }

    // 追加一条结构化日志，按值返回（内部 vector 重分配不会使返回对象失效）
    LogEntry append(
        std::string category,
        std::string message_key,
        std::vector<LogField> fields = {},
        std::string detail = {});

    // 追加事件日志：自动携带事件上下文（source/target/event_id/parent）
    LogEntry append_event(
        std::string category,
        std::string message_key,
        EventId event_id,
        EntityId source,
        EntityId target,
        std::vector<LogField> fields = {},
        std::string detail = {});

    // 调试追踪（level=Trace）
    LogEntry trace(std::string category, std::string message_key, std::string detail = {});

    const std::vector<LogEntry>& entries() const { return entries_; }
    size_t size() const { return entries_.size(); }

    // 渲染全文（每行一条），字段渲染为 key=value
    std::string to_text() const;

    // 渲染单条记录
    static std::string render_entry(const LogEntry& e);

    void clear() { entries_.clear(); seq_ = 0; }

private:
    struct Ctx {
        RoundIndex round_index = 0;
        EntityId current_actor = kInvalidEntity;
        ActionId action_id = kInvalidAction;
    };
    Ctx ctx_;
    SequenceId seq_ = 0;
    std::vector<LogEntry> entries_;
};

// 便捷构造：整数字段
inline LogField f_int(std::string key, int64_t v) { return LogField{std::move(key), v}; }
inline LogField f_str(std::string key, std::string v) { return LogField{std::move(key), std::move(v)}; }
inline LogField f_bool(std::string key, bool v) { return LogField{std::move(key), v}; }

}  // namespace mew::combat
