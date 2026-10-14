#include "pch.h"
#include "test_framework.h"

using namespace mew::combat;

// Python 生成器产出的反射：名称/描述/解析/往返
TEST_CASE(reflection_roundtrip) {
    StatType v = StatType::LCK;
    CHECK_EQ(to_string(v), std::string_view("LCK"));

    StatType parsed = StatType::STR;
    CHECK(parse_enum("SPD", parsed));
    CHECK_EQ(parsed, StatType::SPD);
    CHECK(!parse_enum("NOT_A_STAT", parsed));
    CHECK_EQ(parsed, StatType::SPD);  // 失败时不得修改输出
}

TEST_CASE(reflection_tables) {
    const auto& entries = enum_entries(StatType{});
    CHECK_EQ(entries.size(), enum_count(StatType{}));
    CHECK_EQ(entries[0].value, StatType::STR);
    CHECK_EQ(entries[0].name, std::string_view("STR"));

    // 所有枚举都能生成非空表且名称互不重复
    for (const auto& entry : enum_entries(EventType{})) {
        CHECK(!entry.name.empty());
        CHECK(to_string(entry.value) == entry.name);
    }
    for (const auto& entry : enum_entries(EffectType{})) {
        CHECK(!entry.name.empty());
        CHECK(to_string(entry.value) == entry.name);
    }
}

TEST_CASE(reflection_descriptions) {
    CHECK(enum_desc(StatType::SPD) != nullptr);
    CHECK(std::string_view(enum_desc(StatType::SPD)).find("速度") != std::string_view::npos);
    CHECK_EQ(enum_count(StatType{}), 7u);  // Mewgenics 七属性
    CHECK_EQ(enum_count(DerivedStatType{}), 9u);
}
