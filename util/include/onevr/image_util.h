#pragma once
#include "onevr/frame.h"

#include <string>

namespace onevr {

// Writes RGB24 as binary PPM (P6).
void write_ppm(const std::string& path, const rgb::Frame& frame);

} // namespace onevr
