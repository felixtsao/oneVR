#pragma once
#include "onevr/frame.h"

namespace onevr {

// Side-by-side concatenated frames (stereo)
rgb::Frame cat_sbs(const rgb::Frame& left, const rgb::Frame& right);

} // namespace onevr
