#pragma once

#include "onevr/frame.h"
#include "onevr/uv_map.h"

#include <cstddef>
#include <cstdint>

namespace onevr::cuda {

// Projection map with bilinear sampling with H2D copy io
rgb::Frame project_bilinear(const rgb::Frame& src, const UvMap& lut);

// Projection map with bilinear sampling in-place on GDDR
void project_bilinear(const rgb::Frame& src,
                      uint8_t* d_src,
                      const UvMap& lut,
                      const Uv* d_lut,
                      int lut_x_offset,
                      float contrast,
                      float brightness,
                      uint8_t* out);

} // namespace onevr::cuda
