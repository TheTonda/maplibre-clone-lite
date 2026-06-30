// test_main.cpp — GTest entry point and test runner
#include <gtest/gtest.h>
#include <cstdio>

int main(int argc, char** argv) {
    std::printf("=== Map Renderer v2 — Test Suite ===\n");
    testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    std::printf("\n=== Tests completed: %d ===\n", result);
    return result;
}
