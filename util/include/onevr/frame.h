#pragma once
#include <cstdint>
#include <vector>

namespace onevr {

struct FrameRGB {
    int width = 0;
    int height = 0;
    int stride = 0;                // bytes per row
    std::vector<uint8_t> data;     // RGB24 packed, stride*height bytes
};

}  // namespace onevr
