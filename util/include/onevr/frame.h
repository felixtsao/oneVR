#pragma once
#include <cstdint>
#include <vector>

namespace onevr {
namespace rgb {

struct Frame {
    Frame(){};
    Frame(int w, int h) : width(w), height(h), stride(w * 3), data((size_t)stride * h) {
        data.resize((size_t)w * h * 3);
    }
    int width = 0;
    int height = 0;
    int stride = 0;            // bytes per row
    std::vector<uint8_t> data; // RGB24 packed, stride*height bytes
};

} // namespace rgb
} // namespace onevr
