#pragma once
#include <vector>
#include <stdexcept>

namespace onevr {

template <typename T>
struct Lut2D {  // 2 dimensional look-up table
    int width = 0;
    int height = 0;
    std::vector<T> data;

    Lut2D() = default;
    Lut2D(int width, int height) : width(width), height(height), data((size_t)width * height) {
        if (width <= 0 || height <= 0) throw std::runtime_error("Lut2D: invalid dimension");
    }

    T& at(int x, int y) { return data[(size_t)y * width + x]; }
    const T& at(int x, int y) const { return data[(size_t)y * width + x]; }
};

} // namespace onevr
