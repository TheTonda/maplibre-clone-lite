#include <gtest/gtest.h>
#include "core/camera_ubo.h"
#include <glm/gtc/matrix_transform.hpp>

TEST(CameraUBOTest, Size) {
    EXPECT_EQ(sizeof(CameraUBO), 2 * sizeof(glm::mat4));
}

TEST(CameraUBOTest, LayoutBinding) {
    auto b = CameraUBO::layout_binding();
    EXPECT_EQ(b.binding, 0u);
    EXPECT_EQ(b.descriptorType, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    EXPECT_EQ(b.descriptorCount, 1u);
    EXPECT_EQ(b.stageFlags, VK_SHADER_STAGE_VERTEX_BIT);
}

TEST(CameraUBOTest, BufferInfo) {
    // Create a fake VkBuffer handle (just test that the function compiles
    // and returns correct range)
    VkBuffer fake = nullptr;  // just a placeholder
    auto info = CameraUBO::buffer_info(fake);
    EXPECT_EQ(info.buffer, fake);
    EXPECT_EQ(info.offset, 0u);
    EXPECT_EQ(info.range, sizeof(CameraUBO));
}
