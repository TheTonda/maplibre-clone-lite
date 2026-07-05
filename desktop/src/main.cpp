#include "MapWidget.h"

#include "maprender/c_api.h"

#include <FL/Fl.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#include <cstdlib>

namespace {

void open_cb(Fl_Widget*, void* data) {
    auto* map = static_cast<MapWidget*>(data);
    const char* path = fl_file_chooser(
        "Open WebP raster .mbtiles", "*.mbtiles", nullptr, 0);
    if (!path) return;

    MR_Context* ctx = mr_open(path);
    if (!ctx) {
        fl_alert("Cannot open %s:\n%s", path, mr_last_error(nullptr));
        return;
    }

    if (map->context()) {
        mr_close(map->context());
    }
    map->set_path(path);
    map->set_context(ctx);

    if (Fl_Window* win = static_cast<Fl_Window*>(map->window())) {
        win->redraw();
    }
}

}  // namespace

int main(int argc, char** argv) {
    Fl_Window window(1024, 768, "mapview");
    Fl_Menu_Bar menu(0, 0, 1024, 30);
    MapWidget map(0, 30, 1024, 738);

    menu.add("&File/&Open...", FL_CTRL + 'o', open_cb, &map);

    if (argc > 1) {
        MR_Context* ctx = mr_open(argv[1]);
        if (ctx) {
            map.set_path(argv[1]);
            map.set_context(ctx);
        } else {
            fl_alert("Cannot open %s:\n%s", argv[1], mr_last_error(nullptr));
        }
    }

    window.end();
    window.resizable(map);
    window.show();
    return Fl::run();
}
