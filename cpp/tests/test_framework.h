#pragma once
// ============================================================================
// 极简测试框架（无外部依赖）。
// 用法：
//   TEST_CASE(some_name) { CHECK(expr); CHECK_EQ(a, b); ... }
// main 调用 mewtest::run_all(argc, argv)，返回失败用例数（0 = 全部通过）。
// ============================================================================

#include <cstdio>
#include <exception>
#include <functional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace mewtest {

struct TestFailure {
    std::string message;
};

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

struct Registry {
    std::vector<TestCase> tests;

    static Registry& get() {
        static Registry instance;
        return instance;
    }
    void add(std::string name, std::function<void()> fn) {
        tests.push_back(TestCase{std::move(name), std::move(fn)});
    }
};

[[noreturn]] inline void fail(const char* file, int line, const std::string& msg) {
    std::ostringstream oss;
    oss << file << ":" << line << ": " << msg;
    throw TestFailure{oss.str()};
}

inline int run_all(int argc, char** argv) {
    (void)argc;
    (void)argv;
    int failed = 0;
    const auto& tests = Registry::get().tests;
    for (const auto& test : tests) {
        try {
            std::printf("[RUN ] %s\n", test.name.c_str());
            test.fn();
            std::printf("[PASS] %s\n", test.name.c_str());
        } catch (const TestFailure& f) {
            ++failed;
            std::printf("[FAIL] %s\n       %s\n", test.name.c_str(), f.message.c_str());
        } catch (const std::exception& e) {
            ++failed;
            std::printf("[FAIL] %s\n       unexpected exception: %s\n", test.name.c_str(), e.what());
        }
    }
    std::printf("%zu tests, %d failed\n", tests.size(), failed);
    return failed;
}

}  // namespace mewtest

#define MEW_CONCAT_IMPL(a, b) a##b
#define MEW_CONCAT(a, b) MEW_CONCAT_IMPL(a, b)

#define TEST_CASE_IMPL(name, line)                                            \
    static void MEW_CONCAT(mew_test_fn_, line)();                             \
    static const bool MEW_CONCAT(mew_test_reg_, line) = [] {                  \
        mewtest::Registry::get().add(#name, &MEW_CONCAT(mew_test_fn_, line)); \
        return true;                                                          \
    }();                                                                      \
    static void MEW_CONCAT(mew_test_fn_, line)()

#define TEST_CASE(name) TEST_CASE_IMPL(name, __LINE__)

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) mewtest::fail(__FILE__, __LINE__, "CHECK failed: " #cond); \
    } while (0)

namespace mewtest {

// 可流式输出检测（C++20 concept）
template <typename T>
concept Streamable = requires(std::ostringstream& oss, const T& v) { oss << v; };

// 打印失败值：enum 按底层整数输出；不可流式类型打占位符
template <typename T>
inline void print_value(std::ostringstream& oss, const T& v) {
    if constexpr (std::is_enum_v<std::decay_t<T>>) {
        oss << static_cast<std::underlying_type_t<std::decay_t<T>>>(v);
    } else if constexpr (Streamable<T>) {
        oss << v;
    } else {
        oss << "<unprintable>";
    }
}

}  // namespace mewtest

#define CHECK_EQ(a, b)                                                        \
    do {                                                                      \
        const auto& mew_va = (a);                                             \
        const auto& mew_vb = (b);                                             \
        if (!(mew_va == mew_vb)) {                                            \
            std::ostringstream mew_oss;                                       \
            mew_oss << "CHECK_EQ failed: " #a " == " #b " (";                 \
            mewtest::print_value(mew_oss, mew_va);                            \
            mew_oss << " vs ";                                                \
            mewtest::print_value(mew_oss, mew_vb);                            \
            mew_oss << ")";                                                   \
            mewtest::fail(__FILE__, __LINE__, mew_oss.str());                 \
        }                                                                     \
    } while (0)

#define CHECK_THROWS(expr)                                                    \
    do {                                                                      \
        bool mew_thrown = false;                                              \
        try {                                                                 \
            (void)(expr);                                                     \
        } catch (...) {                                                       \
            mew_thrown = true;                                                \
        }                                                                     \
        if (!mew_thrown) mewtest::fail(__FILE__, __LINE__,                    \
            "CHECK_THROWS failed: " #expr " did not throw");                  \
    } while (0)
