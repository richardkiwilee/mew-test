#include <cstdio>

#include "test_framework.h"

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);  // 崩溃时保留已输出内容
    std::printf("[main] entering run_all\n");
    const int result = mewtest::run_all(argc, argv);
    std::printf("[main] done, failed=%d\n", result);
    return result;
}
