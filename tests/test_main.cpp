#include <gtest/gtest.h>

/// @brief Google Test entry point.
///
/// Initialises the GTest framework and runs all registered tests.
/// Returns 0 on success, non-zero on failure.
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
