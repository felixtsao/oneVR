#pragma once
#include <cstdint>
#include <vector>

namespace onevr {
namespace rgb {

struct Frame {
    int width = 0;
    int height = 0;
    int stride = 0;                // bytes per row
    std::vector<uint8_t> data;     // RGB24 packed, stride*height bytes
};

}  // namespace rgb
}  // namespace onevr
