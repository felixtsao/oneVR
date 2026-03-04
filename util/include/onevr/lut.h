#pragma once
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace onevr {

template <typename T> struct Lut2D { // 2 dimensional look-up table
    int width = 0;
    int height = 0;
    std::vector<T> data;

    Lut2D() = default;

    Lut2D(int w, int h) : width(w), height(h), data((size_t)w * (size_t)h) {
        if (w <= 0 || h <= 0)
            throw std::runtime_error("Lut2D: invalid dimension");
    }

    // Number of elements
    size_t count() const { return (size_t)width * (size_t)height; }

    // Total bytes of the backing array.
    size_t bytes() const { return count() * sizeof(T); }

    // Raw pointer access (useful for CUDA memcpy).
    const T* data_ptr() const { return data.data(); }

    // Optional sanity check before GPU upload or CPU indexing.
    void validate() const {
        if (width <= 0 || height <= 0)
            throw std::runtime_error("Lut2D: invalid dimension");
        if (data.size() != count())
            throw std::runtime_error("Lut2D: data.size != width*height");
    }

    T& at(int x, int y) {
        assert(0 <= x && x < width);
        assert(0 <= y && y < height);
        return data[(size_t)y * (size_t)width + (size_t)x];
    }

    const T& at(int x, int y) const {
        assert(0 <= x && x < width);
        assert(0 <= y && y < height);
        return data[(size_t)y * (size_t)width + (size_t)x];
    }
};

} // namespace onevr
