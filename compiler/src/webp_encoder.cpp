#include "webp_encoder.h"

#include <webp/encode.h>

namespace mapbake {

std::vector<uint8_t> encode_webp(const std::vector<uint8_t>& rgba, float quality) {
    std::vector<uint8_t> out;
    if (rgba.size() != 256u * 256 * 4) return out;
    uint8_t* webp = nullptr;
    const float q = std::max(0.0f, std::min(100.0f, quality));
    size_t n = WebPEncodeRGBA(rgba.data(), 256, 256, 256 * 4, q, &webp);
    if (webp && n > 0) {
        out.assign(webp, webp + n);
        WebPFree(webp);
    }
    return out;
}

}  // namespace mapbake