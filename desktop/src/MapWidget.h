#pragma once

#include "maprender/c_api.h"

#include <FL/Fl_Widget.H>

#include <string>

class MapWidget : public Fl_Widget {
public:
    MapWidget(int x, int y, int w, int h);

    void set_context(MR_Context* ctx);
    void set_path(const std::string& path) { path_ = path; }
    MR_Context* context() const { return ctx_; }

    void draw() override;
    int handle(int event) override;
    void resize(int x, int y, int w, int h) override;

private:
    MR_Context* ctx_ = nullptr;
    int min_zoom_ = 0;
    int max_zoom_ = 20;
    int zoom_ = 0;
    double center_lon_ = 0.0;
    double center_lat_ = 0.0;

    bool dragging_ = false;
    int last_x_ = 0;
    int last_y_ = 0;
    std::string path_;

    void pan_by(int dx, int dy);
    void zoom_by(int wheel_dy, int anchor_x, int anchor_y);
    void screen_to_lon_lat(int sx, int sy, double& lon, double& lat) const;
    void update_view();
};
