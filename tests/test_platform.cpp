// Test that platform interface headers compile and macros work
#include <gtest/gtest.h>
#include <map_renderer/platform.h>
#include <map_renderer/debug_log.h>
#include <map_renderer/gl_check.h>

// Verify PlatformInterface is abstract
TEST(PlatformInterfaceTest, IsAbstract) {
    EXPECT_TRUE(std::is_abstract_v<map_renderer::PlatformInterface>);
}

// Verify GLFunctions is trivial (can be memset to zero)
TEST(GLFunctionsTest, IsTrivial) {
    EXPECT_TRUE(std::is_trivially_copyable_v<map_renderer::GLFunctions>);
}

// Verify InputData is default-constructible
TEST(InputDataTest, DefaultConstructible) {
    map_renderer::InputData d;
    EXPECT_FLOAT_EQ(d.x, 0.0f);
    EXPECT_FLOAT_EQ(d.y, 0.0f);
    EXPECT_FLOAT_EQ(d.delta, 0.0f);
}

// Verify macros compile in both debug and release modes
TEST(MacrosTest, Compile) {
    // DEBUG_LOG should compile (even if it becomes a no-op)
    DEBUG_LOG("test message %d", 42);

    // Create a mock GLFunctions with a fake glGetError
    map_renderer::GLFunctions gl{};
    gl.glGetError = []() -> uint32_t { return 0; };
    (void)gl;  // used by GL_CHECK only in debug builds

    // GL_CHECK should compile and not crash (error = 0)
    GL_CHECK(gl);

    SUCCEED();
}
