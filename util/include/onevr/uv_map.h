#pragma once
#include "onevr/frame.h"
#include "onevr/lut.h"

#include <cstdint>

namespace onevr {

struct Uv {
    float u = 0.f;
    float v = 0.f;
    uint8_t valid = 0;
    uint8_t pad[3];
};
static_assert(sizeof(Uv) == 12);

using UvMap = Lut2D<Uv>;

rgb::Frame project_nearest(const rgb::Frame& in, const UvMap& lut);
rgb::Frame project_bilinear(const rgb::Frame& in, const UvMap& lut);

} // namespace onevr
