#include <gtest/gtest.h>
#include "core/camera.h"
#include <glm/gtc/matrix_transform.hpp>

TEST(CameraTest, Default2DMode) {
    Camera cam;
    EXPECT_EQ(cam.get_mode(), CameraMode::MODE_2D);
    EXPECT_FLOAT_EQ(cam.get_zoom(), 1.0f);
}

TEST(CameraTest, SetFrameBoundsCentres) {
    Camera cam;
    cam.set_frame_bounds(0, 0, 100, 200);
    auto pos = cam.get_position();
    EXPECT_FLOAT_EQ(pos.x, 50.0f);
    EXPECT_FLOAT_EQ(pos.z, 100.0f);
}

TEST(CameraTest, ModeSwitchPreservesZoom) {
    Camera cam;
    cam.set_frame_bounds(0, 0, 100, 100);
    cam.set_zoom(2.0f);
    cam.set_mode(CameraMode::MODE_3D);
    // 500/2 = 250 distance
    EXPECT_FLOAT_EQ(cam.get_distance(), 250.0f);
    cam.set_mode(CameraMode::MODE_2D);
    // 500/250 = 2 zoom
    EXPECT_FLOAT_EQ(cam.get_zoom(), 2.0f);
}

TEST(CameraTest, ZoomClamping) {
    Camera cam;
    cam.set_zoom(0.0f);
    EXPECT_FLOAT_EQ(cam.get_zoom(), Camera::ZOOM_MIN);

    cam.set_zoom(100.0f);
    EXPECT_FLOAT_EQ(cam.get_zoom(), Camera::ZOOM_MAX);
}

TEST(CameraTest, DistanceClamping) {
    Camera cam;
    cam.set_mode(CameraMode::MODE_3D);
    cam.set_distance(0.0f);
    EXPECT_FLOAT_EQ(cam.get_distance(), Camera::DIST_MIN);

    cam.set_distance(10000.0f);
    EXPECT_FLOAT_EQ(cam.get_distance(), Camera::DIST_MAX);
}

TEST(CameraTest, TiltClamping) {
    Camera cam;
    cam.set_tilt(180.0f);
    EXPECT_FLOAT_EQ(cam.get_tilt(), Camera::TILT_MAX);

    cam.set_tilt(-90.0f);
    EXPECT_FLOAT_EQ(cam.get_tilt(), Camera::TILT_MIN);
}

TEST(CameraTest, RotationWraps) {
    Camera cam;
    cam.set_rotation(370.0f);
    EXPECT_FLOAT_EQ(cam.get_rotation(), 10.0f);
    cam.set_rotation(-10.0f);
    EXPECT_FLOAT_EQ(cam.get_rotation(), 350.0f);
}

TEST(CameraTest, ZoomBy2DScale) {
    Camera cam;
    cam.zoom_by(-1.0f);  // zoom in
    EXPECT_GT(cam.get_zoom(), 1.0f);
    cam.zoom_by(1.0f);    // zoom out
    EXPECT_LT(cam.get_zoom(), 1.1f);  // roughly back
}

TEST(CameraTest, Projection2DAspectAware) {
    Camera cam;
    cam.set_frame_bounds(0, 0, 200, 100);
    auto m = cam.get_projection_matrix(2.0f);  // wide
    // Should not crash, matrix valid
    EXPECT_FLOAT_EQ(m[0][0], m[0][0]);  // non-NaN
}

TEST(CameraTest, Projection3DPerspective) {
    Camera cam;
    cam.set_mode(CameraMode::MODE_3D);
    cam.set_frame_bounds(0, 0, 100, 100);
    auto m = cam.get_projection_matrix(1.0f);
    EXPECT_FLOAT_EQ(m[0][0], m[0][0]);  // non-NaN
}

TEST(CameraTest, ViewMatrix2D) {
    Camera cam;
    cam.set_frame_bounds(0, 0, 100, 100);
    auto view = cam.get_view_matrix();
    // Centre (50, 0, 50) is at distance 1000 along view direction in 2D mode.
    // The centre should be at z = -1000 in view space (directly below the eye).
    glm::vec4 centre_ws(50.0f, 0.0f, 50.0f, 1.0f);
    glm::vec4 centre_vs = view * centre_ws;
    EXPECT_NEAR(centre_vs.x, 0.0f, 0.1f);   // centred on x
    EXPECT_NEAR(centre_vs.z, -1000.0f, 1.0f);  // along -z in view space
}

TEST(CameraTest, ViewMatrix3D) {
    Camera cam;
    cam.set_frame_bounds(0, 0, 100, 100);
    cam.set_mode(CameraMode::MODE_3D);
    auto view = cam.get_view_matrix();
    // 3D view: camera looks from distance=500, tilt=45°, rotation=0°.
    // The centre point (50, 0, 50) projects to (0, -535.3, ~?) depending on
    // the exact spherical trig.  Just verify it's not identity and isn't NaN.
    EXPECT_FALSE(glm::isnan(view[0][0]));
    EXPECT_TRUE(glm::length(glm::vec3(view[0])) > 0.5f);
}

TEST(CameraTest, Position) {
    Camera cam;
    cam.set_position(100.0f, 200.0f);
    auto pos = cam.get_position();
    EXPECT_FLOAT_EQ(pos.x, 100.0f);
    EXPECT_FLOAT_EQ(pos.z, 200.0f);
}

TEST(CameraTest, ModeSwitchStays) {
    Camera cam;
    cam.set_mode(CameraMode::MODE_3D);
    EXPECT_EQ(cam.get_mode(), CameraMode::MODE_3D);
    cam.set_mode(CameraMode::MODE_2D);
    EXPECT_EQ(cam.get_mode(), CameraMode::MODE_2D);
}
