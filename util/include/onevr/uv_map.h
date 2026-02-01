#pragma once
#include <cstdint>
#include "onevr/lut.h"
#include "onevr/frame.h"

namespace onevr {

struct Uv {
    float u = 0.f;
    float v = 0.f;
    uint8_t valid = false;
};

using UvMap = Lut2D<Uv>;

rgb::Frame project_nearest(const rgb::Frame& in, const UvMap& lut);
rgb::Frame project_bilinear(const rgb::Frame& in, const UvMap& lut);

} // namespace onevr
