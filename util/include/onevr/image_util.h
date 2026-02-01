#pragma once
#include <string>
#include "onevr/frame.h"

namespace onevr {

// Writes RGB24 as binary PPM (P6).
void write_ppm(const std::string& path, const FrameRGB& frame);

}  // namespace onevr
