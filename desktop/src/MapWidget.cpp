#include "MapWidget.h"

#include "maprender/mercator.h"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <cmath>
#include <cstdio>

MapWidget::MapWidget(int x, int y, int w, int h)
    : Fl_Widget(x, y, w, h, "map") {
}

void MapWidget::set_context(MR_Context* ctx) {
    ctx_ = ctx;
    if (!ctx_) return;

    min_zoom_ = mr_min_zoom(ctx_);
    max_zoom_ = mr_max_zoom(ctx_);

    double w, s, e, n;
    mr_bounds(ctx_, &w, &s, &e, &n);
    center_lon_ = (w + e) / 2.0;
    center_lat_ = (s + n) / 2.0;
    zoom_ = max_zoom_;

    update_view();
    redraw();
}

void MapWidget::update_view() {
    if (!ctx_) return;
    mr_set_view(ctx_, center_lon_, center_lat_, zoom_, w(), h());
}

void MapWidget::draw() {
    if (!ctx_) {
        fl_color(FL_GRAY);
        fl_rectf(x(), y(), w(), h());
        return;
    }

    update_view();
    const MR_Frame* frame = mr_render(ctx_);
    const int fw = mr_frame_width(frame);
    const int fh = mr_frame_height(frame);
    const unsigned char* px = mr_frame_pixels(frame);
    if (px && fw > 0 && fh > 0) {
        fl_draw_image(px, x(), y(), fw, fh, 4);
    }

    if (window()) {
        char title[256];
        const char* name = path_.empty() ? "mapview" : path_.c_str();
        std::snprintf(title, sizeof(title), "%s — z%d", name, zoom_);
        window()->label(title);
    }
}

int MapWidget::handle(int event) {
    if (!ctx_) return Fl_Widget::handle(event);

    switch (event) {
        case FL_PUSH:
            if (Fl::event_button() == FL_LEFT_MOUSE) {
                dragging_ = true;
                last_x_ = Fl::event_x();
                last_y_ = Fl::event_y();
                return 1;
            }
            break;

        case FL_DRAG:
            if (dragging_) {
                const int dx = Fl::event_x() - last_x_;
                const int dy = Fl::event_y() - last_y_;
                last_x_ = Fl::event_x();
                last_y_ = Fl::event_y();
                pan_by(dx, dy);
                redraw();
                return 1;
            }
            break;

        case FL_RELEASE:
            dragging_ = false;
            return 1;

        case FL_MOUSEWHEEL: {
            const int dy = Fl::event_dy();
            if (dy != 0) {
                zoom_by(dy, Fl::event_x() - x(), Fl::event_y() - y());
                redraw();
            }
            return 1;
        }
    }

    return Fl_Widget::handle(event);
}

void MapWidget::resize(int x, int y, int w, int h) {
    Fl_Widget::resize(x, y, w, h);
    update_view();
    redraw();
}

void MapWidget::screen_to_lon_lat(int sx, int sy,
                                  double& lon, double& lat) const {
    const double cx = maprender::lon_to_world_x(center_lon_, zoom_);
    const double cy = maprender::lat_to_world_y(center_lat_, zoom_);
    const double left = cx - w() / 2.0;
    const double top  = cy - h() / 2.0;
    lon = maprender::world_x_to_lon(left + sx, zoom_);
    lat = maprender::world_y_to_lat(top  + sy, zoom_);
}

void MapWidget::pan_by(int dx, int dy) {
    double cx = maprender::lon_to_world_x(center_lon_, zoom_);
    double cy = maprender::lat_to_world_y(center_lat_, zoom_);
    cx -= dx;
    cy -= dy;

    const double W = maprender::world_width_at(zoom_);
    const double half_w = w() / 2.0;
    const double half_h = h() / 2.0;
    cx = std::clamp(cx, half_w, std::max(half_w, W - half_w));
    cy = std::clamp(cy, half_h, std::max(half_h, W - half_h));

    center_lon_ = maprender::world_x_to_lon(cx, zoom_);
    center_lat_ = maprender::world_y_to_lat(cy, zoom_);
}

void MapWidget::zoom_by(int wheel_dy, int anchor_x, int anchor_y) {
    int new_zoom = zoom_ - wheel_dy;
    new_zoom = std::clamp(new_zoom, min_zoom_, max_zoom_);
    if (new_zoom == zoom_) return;

    double anchor_lon, anchor_lat;
    screen_to_lon_lat(anchor_x, anchor_y, anchor_lon, anchor_lat);

    const double ax_new = maprender::lon_to_world_x(anchor_lon, new_zoom);
    const double ay_new = maprender::lat_to_world_y(anchor_lat, new_zoom);

    double new_cx = ax_new - (anchor_x - w() / 2.0);
    double new_cy = ay_new - (anchor_y - h() / 2.0);

    const double W = maprender::world_width_at(new_zoom);
    const double half_w = w() / 2.0;
    const double half_h = h() / 2.0;
    new_cx = std::clamp(new_cx, half_w, std::max(half_w, W - half_w));
    new_cy = std::clamp(new_cy, half_h, std::max(half_h, W - half_h));

    zoom_ = new_zoom;
    center_lon_ = maprender::world_x_to_lon(new_cx, zoom_);
    center_lat_ = maprender::world_y_to_lat(new_cy, zoom_);
}
