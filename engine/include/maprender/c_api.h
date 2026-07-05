#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MR_Context MR_Context;
typedef struct MR_Frame   MR_Frame;

MR_Context* mr_open(const char* mbtiles_path);
void        mr_close(MR_Context* ctx);

int         mr_min_zoom(MR_Context* ctx);
int         mr_max_zoom(MR_Context* ctx);
void        mr_bounds(MR_Context* ctx, double* w, double* s, double* e, double* n);
const char* mr_last_error(MR_Context* ctx);

void mr_set_view(MR_Context* ctx, double lon, double lat,
                 int zoom_int, int screen_w, int screen_h);
void mr_pan(MR_Context* ctx, int dx_px, int dy_px);
void mr_zoom(MR_Context* ctx, int delta,
             double anchor_lon, double anchor_lat);

const MR_Frame* mr_render(MR_Context* ctx);
const unsigned char* mr_frame_pixels(const MR_Frame* f);
int  mr_frame_width(const MR_Frame* f);
int  mr_frame_height(const MR_Frame* f);

#ifdef __cplusplus
}
#endif