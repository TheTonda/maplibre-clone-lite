#include "webp_decoder.h"

#include <webp/decode.h>

namespace maprender {

bool webp_decode_rgba(const unsigned char* blob, size_t bytes,
                      int& w, int& h, std::vector<unsigned char>& out) {
    if (!blob || bytes == 0) return false;
    if (!WebPGetInfo(blob, bytes, &w, &h)) return false;
    out.resize(static_cast<size_t>(w) * h * 4);
    WebPDecoderConfig cfg;
    WebPInitDecoderConfig(&cfg);
    cfg.output.colorspace = MODE_RGBA;
    cfg.output.u.RGBA.rgba = out.data();
    cfg.output.u.RGBA.stride = w * 4;
    cfg.output.u.RGBA.size = out.size();
    if (WebPDecode(blob, bytes, &cfg) != VP8_STATUS_OK) return false;
    return true;
}

}  // namespace maprender