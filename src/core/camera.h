#pragma once

/// @file camera.h
/// @brief Camera supporting 2D orthographic and 3D perspective modes.

#include <glm/glm.hpp>
#include <algorithm>
#include "core/input_state.h"

/// Camera mode: 2D top-down or 3D free-look.
enum class CameraMode { MODE_2D, MODE_3D };

/// Controls view transformation: position, zoom, distance, tilt, rotation.
///
/// 2D mode uses orthographic projection centred on (x, z) at a zoom level.
/// 3D mode uses perspective projection with spherical coordinates
/// (distance, tilt, rotation) around the look-at point (x, 0, z).
class Camera {
public:
    /// Set the data extent so initial camera fits the entire dataset.
    void set_frame_bounds(double min_x, double min_z,
                          double max_x, double max_z);

    /// @name Mode
    ///@{
    CameraMode get_mode() const { return mode_; }
    void       set_mode(CameraMode mode);
    ///@}

    /// @name Position (look-at point in ENU metres)
    ///@{
    glm::vec3 get_position() const { return {x_, 0.0f, z_}; }
    void      set_position(float x, float z) { x_ = x; z_ = z; }
    ///@}

    /// @name Zoom (2D) / Distance (3D)
    ///@{
    float get_zoom()     const { return zoom_; }
    void  set_zoom(float z)    { zoom_ = std::clamp(z, ZOOM_MIN, ZOOM_MAX); }

    float get_distance() const { return distance_; }
    void  set_distance(float d) { distance_ = std::clamp(d, DIST_MIN, DIST_MAX); }

    void zoom_by(float delta);
    ///@}

    /// @name Tilt (3D, degrees)
    ///@{
    float get_tilt() const { return tilt_; }
    void  set_tilt(float t)   { tilt_ = std::clamp(t, TILT_MIN, TILT_MAX); }
    ///@}

    /// @name Rotation (3D, degrees)
    ///@{
    float get_rotation() const { return rotation_; }
    void  set_rotation(float r) { rotation_ = std::fmod(r, 360.0f); if (rotation_ < 0) rotation_ += 360.0f; }
    ///@}

    /// Populate the camera uniform buffer (proj × view) for the GPU.
    glm::mat4 get_projection_matrix(float aspect) const;
    glm::mat4 get_view_matrix() const;

    /// Read input state and update camera parameters.
    void update_from_input(const InputState& state);

    // ---- Constants ----
    static constexpr float ZOOM_MIN  = 0.1f;
    static constexpr float ZOOM_MAX  = 20.0f;
    static constexpr float DIST_MIN  = 50.0f;
    static constexpr float DIST_MAX  = 5000.0f;
    static constexpr float TILT_MIN  = 0.0f;
    static constexpr float TILT_MAX  = 85.0f;

private:
    CameraMode mode_ = CameraMode::MODE_2D;

    // Look-at point (ENU metres)
    float x_ = 0.0f, z_ = 0.0f;

    // Data bounds
    float data_min_x_ = -500.0f, data_max_x_ = 500.0f;
    float data_min_z_ = -500.0f, data_max_z_ = 500.0f;

    // 2D state
    float zoom_ = 1.0f;

    // 3D state
    float distance_ = 500.0f;
    float tilt_     = 45.0f;
    float rotation_ = 0.0f;
};
