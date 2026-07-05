#include "webp_decoder.h"

#include <webp/decode.h>

namespace maprender {

bool webp_decode_rgba(const unsigned char* blob, size_t bytes,
                      int& w, int& h, std::vector<unsigned char>& out) {
    if (!blob || bytes == 0) return false;
    if (!WebPGetInfo(blob, bytes, &w, &h)) return false;
    out.resize(static_cast<size_t>(w) * h * 4);
    if (!WebPDecodeRGBAInto(blob, bytes, out.data(), out.size(), w * 4))
        return false;
    return true;
}

}  // namespace maprender