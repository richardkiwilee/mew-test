#include "pch.h"
#include "test_framework.h"

#include "combat/infrastructure/combat_log.h"

using namespace mew::combat;

// 序号严格递增，上下文快照正确
TEST_CASE(log_sequence_and_context) {
    CombatLog log;
    log.set_round(2);
    log.set_current_actor(7);
    log.set_action(100);

    const auto& e1 = log.append("Scheduler", "TurnStarted");
    const auto& e2 = log.append("Scheduler", "TurnEnded");
    CHECK_EQ(e1.sequence, 1u);
    CHECK_EQ(e2.sequence, 2u);
    CHECK_EQ(e1.round_index, 2u);
    CHECK_EQ(e1.current_actor, 7u);
    CHECK_EQ(e1.action_id, 100u);
}

// 事件日志携带来源/目标/事件 ID
TEST_CASE(log_event_context) {
    CombatLog log;
    const auto& e = log.append_event(
        "Damage", "DamageApplied", /*event_id=*/5u,
        /*source=*/1u, /*target=*/2u,
        {f_int("amount", 12), f_str("dmg_type", "PHYSICAL")});
    CHECK_EQ(e.event_id, 5u);
    CHECK_EQ(e.source_entity, 1u);
    CHECK_EQ(e.target_entity, 2u);
    CHECK_EQ(e.fields.size(), size_t(2));
    CHECK_EQ(e.message_key, std::string("DamageApplied"));
}

// 文本渲染：序号、轮次、key、字段
TEST_CASE(log_render) {
    CombatLog log;
    log.set_round(1);
    log.append("Damage", "DamageApplied", {f_int("amount", 12)});
    log.trace("Trigger", "ConditionFailed", "owner hp ratio too high");
    const std::string text = log.to_text();

    CHECK(text.find("[00001] R1 [Damage] DamageApplied amount=12") != std::string::npos);
    CHECK(text.find("[00002] TRACE R1 [Trigger] ConditionFailed owner hp ratio too high")
          != std::string::npos);
}

// 结构化字段三种类型渲染
TEST_CASE(log_field_types) {
    CombatLog log;
    log.append("Status", "StatusApplied",
        {f_int("stacks", 3), f_str("status", "burn"), f_bool("refreshed", true)});
    const std::string text = log.to_text();
    CHECK(text.find("stacks=3") != std::string::npos);
    CHECK(text.find("status=burn") != std::string::npos);
    CHECK(text.find("refreshed=true") != std::string::npos);
}
