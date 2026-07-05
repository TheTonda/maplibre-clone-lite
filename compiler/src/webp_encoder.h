#pragma once

#include <cstdint>
#include <vector>

namespace mapbake {

// Encode RGBA8 buffer (must be 256x256) to WebP. quality 0..100.
std::vector<uint8_t> encode_webp(const std::vector<uint8_t>& rgba, float quality);

// Lossless WebP encode (larger, but avoids block artifacts at high zoom).
std::vector<uint8_t> encode_webp_lossless(const std::vector<uint8_t>& rgba);

}  // namespace mapbake