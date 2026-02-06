#pragma once

#include <cstddef>
#include <cstdint>

#include "onevr/uv_map.h"
#include "onevr/frame.h"

namespace onevr::cuda {

// Projection map with bilinear sampling with H2D copy io
rgb::Frame project_bilinear(const rgb::Frame& src, const UvMap& lut);

// Projection map with bilinear sampling in-place on GDDR
void project_bilinear(const rgb::Frame& src, const UvMap& lut, uint8_t* out);

}  // namespace onevr::cuda
