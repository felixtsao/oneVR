#pragma once
#include "onevr/frame.h"

namespace onevr {

// Resize RGB24 image to target size
FrameRGB scale_rgb24(const FrameRGB& in, int out_w, int out_h);

// Side-by-side stack (stereo)
FrameRGB sbs_rgb(const FrameRGB& left, const FrameRGB& right);

}
