#include <gtest/gtest.h>
#include "render/renderer.h"
#include "core/vulkan_context.h"
#include "core/window.h"
#include "core/camera.h"
#include "data/geometry_builder.h"

/// Helper: create a minimal environment to test Renderer construction.
/// The tests here are trivial because a full Vulkan device is required for
/// real pipeline/buffer creation — that's covered by the integration test
/// (renderer runs as the main executable).
TEST(RendererTest, Skeleton) {
    // Construction / destruction is safe without initialise.
    Renderer r;
    (void)r;
    // No crash = pass.
}
