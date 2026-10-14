#include "pch.h"
#include "combat/infrastructure/combat_log.h"

namespace mew::combat {

LogEntry CombatLog::append(
    std::string category, std::string message_key, std::vector<LogField> fields, std::string detail) {
    LogEntry e;
    e.sequence = ++seq_;
    e.level = LogLevel::Info;
    e.round_index = ctx_.round_index;
    e.current_actor = ctx_.current_actor;
    e.action_id = ctx_.action_id;
    e.category = std::move(category);
    e.message_key = std::move(message_key);
    e.fields = std::move(fields);
    e.detail = std::move(detail);
    LogEntry copy = e;
    entries_.push_back(std::move(e));
    return copy;
}

LogEntry CombatLog::append_event(
    std::string category, std::string message_key, EventId event_id,
    EntityId source, EntityId target, std::vector<LogField> fields, std::string detail) {
    LogEntry e;
    e.sequence = ++seq_;
    e.level = LogLevel::Info;
    e.round_index = ctx_.round_index;
    e.current_actor = ctx_.current_actor;
    e.action_id = ctx_.action_id;
    e.event_id = event_id;
    e.source_entity = source;
    e.target_entity = target;
    e.category = std::move(category);
    e.message_key = std::move(message_key);
    e.fields = std::move(fields);
    e.detail = std::move(detail);
    LogEntry copy = e;
    entries_.push_back(std::move(e));
    return copy;
}

LogEntry CombatLog::trace(std::string category, std::string message_key, std::string detail) {
    LogEntry e;
    e.sequence = ++seq_;
    e.level = LogLevel::Trace;
    e.round_index = ctx_.round_index;
    e.current_actor = ctx_.current_actor;
    e.action_id = ctx_.action_id;
    e.category = std::move(category);
    e.message_key = std::move(message_key);
    e.detail = std::move(detail);
    LogEntry copy = e;
    entries_.push_back(std::move(e));
    return copy;
}

namespace {

void append_field_text(std::string& out, const LogField& field) {
    out += field.key;
    out += '=';
    if (const auto* i = std::get_if<int64_t>(&field.value)) {
        out += std::format("{}", *i);
    } else if (const auto* s = std::get_if<std::string>(&field.value)) {
        out += *s;
    } else if (const auto* b = std::get_if<bool>(&field.value)) {
        out += *b ? "true" : "false";
    } else {
        out += "?";
    }
}

}  // namespace

std::string CombatLog::render_entry(const LogEntry& e) {
    std::string out = std::format("[{:05}] ", e.sequence);
    if (e.level == LogLevel::Trace) {
        out += "TRACE ";
    }
    if (e.round_index > 0) {
        out += std::format("R{} ", e.round_index);
    }
    if (e.current_actor != kInvalidEntity) {
        out += std::format("actor={} ", e.current_actor);
    }
    if (e.action_id != kInvalidAction) {
        out += std::format("action={} ", e.action_id);
    }
    if (e.event_id != kInvalidEvent) {
        out += std::format("evt={} ", e.event_id);
    }
    if (!e.category.empty()) {
        out += std::format("[{}] ", e.category);
    }
    out += e.message_key;
    if (!e.detail.empty()) {
        out += " ";
        out += e.detail;
    } else {
        for (const auto& field : e.fields) {
            out += " ";
            append_field_text(out, field);
        }
    }
    return out;
}

std::string CombatLog::to_text() const {
    std::string out;
    for (const auto& e : entries_) {
        out += render_entry(e);
        out += '\n';
    }
    return out;
}

}  // namespace mew::combat
