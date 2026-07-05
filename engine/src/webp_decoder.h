#pragma once

#include <cstddef>
#include <vector>

namespace maprender {

// Decodes a WebP blob into a tightly packed RGBA8 buffer of wxh.
// Returns false on failure. The buffer is resized to w*h*4.
bool webp_decode_rgba(const unsigned char* blob, size_t bytes,
                      int& w, int& h, std::vector<unsigned char>& out);

}  // namespace maprender