#include "pch.h"
#include "test_framework.h"

#include "combat/infrastructure/rng_service.h"

using namespace mew::combat;

// 同 seed 同调用序列 => 同结果序列（设计文档 §13 验收）
TEST_CASE(rng_same_seed_reproducible) {
    RngService a(12345);
    RngService b(12345);
    for (int i = 0; i < 100; ++i) {
        CHECK_EQ(a.next_u32(6, "die"), b.next_u32(6, "die"));
    }
}

// 不同 seed => 序列不同（至少前几个值不同）
TEST_CASE(rng_different_seed_differs) {
    RngService a(1);
    RngService b(2);
    bool any_diff = false;
    for (int i = 0; i < 20; ++i) {
        if (a.next_u32(1000, "x") != b.next_u32(1000, "x")) any_diff = true;
    }
    CHECK(any_diff);
}

// 范围校验
TEST_CASE(rng_range_bounds) {
    RngService rng(777);
    for (int i = 0; i < 200; ++i) {
        const uint32_t v = rng.next_u32(5, "bound");
        CHECK(v < 5);
    }
    for (int i = 0; i < 200; ++i) {
        const int32_t v = rng.next_range(-3, 3, "range");
        CHECK(v >= -3 && v <= 3);
    }
}

// 概率判定：0/100% 不消耗随机数序列（保证回放序列稳定）
TEST_CASE(rng_chance_extremes_no_consumption) {
    RngService rng(42);
    const size_t calls_before = rng.calls().size();
    CHECK(!rng.roll_chance(0, "impossible"));
    CHECK(rng.roll_chance(kPercentBase, "certain"));
    CHECK_EQ(rng.calls().size(), calls_before + 2);  // 记录判定但不推进 raw 序列

    // raw 序列未被推进：与"纯序列"对照
    RngService ref(42);
    CHECK_EQ(rng.next_u32(1000, "after"), ref.next_u32(1000, "after"));
}

// 调用审计：call_id 递增、reason 记录
TEST_CASE(rng_call_audit) {
    RngService rng(9);
    rng.next_u32(10, "die");
    rng.roll_chance(5000, "crit");
    rng.next_range(1, 6, "damage");
    const auto& calls = rng.calls();
    CHECK_EQ(calls.size(), size_t(3));
    CHECK_EQ(calls[0].call_id, 1u);
    CHECK_EQ(calls[1].call_id, 2u);
    CHECK_EQ(calls[2].call_id, 3u);
    CHECK_EQ(calls[0].reason, std::string("die"));
    CHECK_EQ(calls[1].reason, std::string("crit"));
    CHECK_EQ(calls[2].reason, std::string("damage"));
}
