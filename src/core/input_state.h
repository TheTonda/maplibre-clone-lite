#pragma once

/// @file input_state.h
/// @brief Per-frame input state populated by Window::poll_events.
///
/// The renderer loop reads this struct once per frame and resets
/// single-frame flags (pressed/released/deltas) before polling again.

struct InputState {
    // ---- Keyboard ----
    bool up            = false;
    bool down          = false;
    bool left          = false;
    bool right         = false;
    bool zoom_in       = false;
    bool zoom_out      = false;
    bool tilt_up       = false;
    bool tilt_down     = false;
    bool rotate_left   = false;
    bool rotate_right  = false;
    bool switch_2d     = false;
    bool switch_3d     = false;

    // ---- Mouse ----
    bool  left_mouse_down = false;
    bool  mouse_pressed   = false;   ///< True only on the frame the button went down.
    bool  mouse_released  = false;   ///< True only on the frame the button came up.
    int   mouse_x         = 0;
    int   mouse_y         = 0;
    int   delta_x         = 0;
    int   delta_y         = 0;
    float scroll_delta    = 0.0f;

    // ---- Time ----
    float dt = 0.0f;                ///< Seconds since last frame.

    /// Reset single-frame state.  Call once at the end of each frame
    /// after the renderer has consumed the input.
    void reset_frame_state() {
        mouse_pressed  = false;
        mouse_released = false;
        delta_x        = 0;
        delta_y        = 0;
        scroll_delta   = 0.0f;
    }
};
