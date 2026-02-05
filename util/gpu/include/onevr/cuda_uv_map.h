#pragma once

#include <cstddef>
#include <cstdint>

#include "onevr/uv_map.h"
#include "onevr/frame.h"

namespace onevr::cuda {

rgb::Frame project_bilinear(const rgb::Frame& src, const UvMap& lut);

}  // namespace onevr::cuda
