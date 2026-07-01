/// @file camera.cpp
/// @brief Camera implementation.

#include "core/camera.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

// -----------------------------------------------------------------------
// Mode switching
// -----------------------------------------------------------------------

void Camera::set_mode(CameraMode mode) {
    if (mode == mode_) return;

    if (mode == CameraMode::MODE_2D) {
        // Switch to 2D: preserve zoom from current distance
        zoom_ = std::clamp(500.0f / distance_, ZOOM_MIN, ZOOM_MAX);
    } else {
        // Switch to 3D: preserve distance from current zoom
        distance_ = std::clamp(500.0f / zoom_, DIST_MIN, DIST_MAX);
        if (tilt_ < 5.0f) tilt_ = 30.0f;  // ensure some tilt
    }
    mode_ = mode;
}

// -----------------------------------------------------------------------
// Frame bounds
// -----------------------------------------------------------------------

void Camera::set_frame_bounds(double min_x, double min_z,
                               double max_x, double max_z)
{
    data_min_x_ = static_cast<float>(min_x);
    data_max_x_ = static_cast<float>(max_x);
    data_min_z_ = static_cast<float>(min_z);
    data_max_z_ = static_cast<float>(max_z);

    // Centre on data
    x_ = (data_min_x_ + data_max_x_) / 2.0f;
    z_ = (data_min_z_ + data_max_z_) / 2.0f;
}

// -----------------------------------------------------------------------
// Zoom
// -----------------------------------------------------------------------

void Camera::zoom_by(float delta) {
    if (mode_ == CameraMode::MODE_2D) {
        zoom_ = std::clamp(zoom_ * (1.0f - delta * 0.1f), ZOOM_MIN, ZOOM_MAX);
    } else {
        distance_ = std::clamp(distance_ * (1.0f - delta * 0.1f),
                               DIST_MIN, DIST_MAX);
    }
}

// -----------------------------------------------------------------------
// Matrices
// -----------------------------------------------------------------------

glm::mat4 Camera::get_projection_matrix(float aspect) const {
    if (mode_ == CameraMode::MODE_2D) {
        float half_x = (data_max_x_ - data_min_x_) / (2.0f * zoom_);
        float half_z = (data_max_z_ - data_min_z_) / (2.0f * zoom_);
        // At least 100 metres visible
        half_x = std::max(half_x, 50.0f);
        half_z = std::max(half_z, 50.0f);

        // Adjust for aspect ratio
        if (aspect > 1.0f) half_z /= aspect;
        else               half_x *= (1.0f / aspect);

        return glm::ortho(-half_x, half_x, -half_z, half_z, -1000.0f, 1000.0f);
    } else {
        return glm::perspective(glm::radians(60.0f), aspect, 0.1f, 10000.0f);
    }
}

glm::mat4 Camera::get_view_matrix() const {
    if (mode_ == CameraMode::MODE_2D) {
        // 2D: look straight down at (x, 0, z)
        return glm::lookAt(
            glm::vec3(x_, 1000.0f, z_),    // eye: high above
            glm::vec3(x_, 0.0f, z_),       // centre: look-at
            glm::vec3(0.0f, 0.0f, -1.0f)   // up: pointing north (negative Z)
        );
    } else {
        // 3D: spherical coordinates around look-at
        float tilt_rad   = glm::radians(tilt_);
        float rot_rad    = glm::radians(rotation_);
        float eye_x = x_ + distance_ * std::cos(tilt_rad) * std::sin(rot_rad);
        float eye_y =       distance_ * std::sin(tilt_rad);
        float eye_z = z_ + distance_ * std::cos(tilt_rad) * std::cos(rot_rad);

        return glm::lookAt(
            glm::vec3(eye_x, eye_y, eye_z),
            glm::vec3(x_, 0.0f, z_),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
    }
}

// -----------------------------------------------------------------------
// Input handling
// -----------------------------------------------------------------------

void Camera::update_from_input(const InputState& state) {
    const float pan_speed_2d = 50.0f / zoom_;
    const float pan_speed_3d = distance_ * 0.002f;

    // Mode switch
    if (state.switch_2d) set_mode(CameraMode::MODE_2D);
    if (state.switch_3d) set_mode(CameraMode::MODE_3D);

    if (mode_ == CameraMode::MODE_2D) {
        // Pan
        if (state.up)    z_ -= pan_speed_2d * state.dt;
        if (state.down)  z_ += pan_speed_2d * state.dt;
        if (state.left)  x_ -= pan_speed_2d * state.dt;
        if (state.right) x_ += pan_speed_2d * state.dt;

        // Mouse drag pan
        if (state.left_mouse_down) {
            x_ -= state.delta_x * pan_speed_2d * 0.01f;
            z_ -= state.delta_y * pan_speed_2d * 0.01f;
        }

        // Zoom
        if (state.zoom_in)  zoom_by(-1.0f);
        if (state.zoom_out) zoom_by(1.0f);
        if (state.scroll_delta != 0.0f) zoom_by(-state.scroll_delta);
    } else {
        // Pan (arrow keys move look-at in world space)
        if (state.up)    z_ -= pan_speed_3d * state.dt;
        if (state.down)  z_ += pan_speed_3d * state.dt;
        if (state.left)  x_ -= pan_speed_3d * state.dt;
        if (state.right) x_ += pan_speed_3d * state.dt;

        // Mouse drag = orbit
        if (state.left_mouse_down) {
            rotation_ += state.delta_x * 0.2f;
            tilt_      = std::clamp(tilt_ - state.delta_y * 0.2f,
                                    TILT_MIN, TILT_MAX);
        }

        // Zoom (scroll = distance)
        if (state.zoom_in)  zoom_by(-1.0f);
        if (state.zoom_out) zoom_by(1.0f);
        if (state.scroll_delta != 0.0f) zoom_by(-state.scroll_delta);

        // Tilt
        if (state.tilt_up)   tilt_ = std::clamp(tilt_ - 30.0f * state.dt,
                                                  TILT_MIN, TILT_MAX);
        if (state.tilt_down) tilt_ = std::clamp(tilt_ + 30.0f * state.dt,
                                                  TILT_MIN, TILT_MAX);

        // Rotate
        if (state.rotate_left)  rotation_ -= 60.0f * state.dt;
        if (state.rotate_right) rotation_ += 60.0f * state.dt;
    }

    // Clamp to data bounds (with some margin)
    float margin_x = (data_max_x_ - data_min_x_) * 0.1f;
    float margin_z = (data_max_z_ - data_min_z_) * 0.1f;
    x_ = std::clamp(x_, data_min_x_ - margin_x, data_max_x_ + margin_x);
    z_ = std::clamp(z_, data_min_z_ - margin_z, data_max_z_ + margin_z);
}
