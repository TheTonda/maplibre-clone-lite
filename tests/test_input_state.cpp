#include <gtest/gtest.h>
#include "core/input_state.h"

TEST(InputStateTest, DefaultState) {
    InputState s;
    EXPECT_FALSE(s.up);
    EXPECT_FALSE(s.down);
    EXPECT_FALSE(s.left);
    EXPECT_FALSE(s.right);
    EXPECT_FALSE(s.zoom_in);
    EXPECT_FALSE(s.zoom_out);
    EXPECT_FALSE(s.tilt_up);
    EXPECT_FALSE(s.tilt_down);
    EXPECT_FALSE(s.rotate_left);
    EXPECT_FALSE(s.rotate_right);
    EXPECT_FALSE(s.switch_2d);
    EXPECT_FALSE(s.switch_3d);
    EXPECT_FALSE(s.left_mouse_down);
    EXPECT_FALSE(s.mouse_pressed);
    EXPECT_FALSE(s.mouse_released);
    EXPECT_EQ(s.mouse_x, 0);
    EXPECT_EQ(s.mouse_y, 0);
    EXPECT_EQ(s.delta_x, 0);
    EXPECT_EQ(s.delta_y, 0);
    EXPECT_FLOAT_EQ(s.scroll_delta, 0.0f);
    EXPECT_FLOAT_EQ(s.dt, 0.0f);
}

TEST(InputStateTest, ResetFrameState) {
    InputState s;
    // Pretend some events happened
    s.mouse_pressed  = true;
    s.mouse_released = true;
    s.delta_x        = 42;
    s.delta_y        = -7;
    s.scroll_delta   = 1.5f;

    s.reset_frame_state();

    // Single-frame flags should be cleared
    EXPECT_FALSE(s.mouse_pressed);
    EXPECT_FALSE(s.mouse_released);
    EXPECT_EQ(s.delta_x, 0);
    EXPECT_EQ(s.delta_y, 0);
    EXPECT_FLOAT_EQ(s.scroll_delta, 0.0f);

    // Persistent state unchanged
    EXPECT_FALSE(s.up);
    EXPECT_FALSE(s.left_mouse_down);
}
