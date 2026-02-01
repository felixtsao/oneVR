#pragma once
#include "onevr/frame.h"

namespace onevr {

// Side-by-side concatenated frames (stereo)
FrameRGB sbs_rgb(const FrameRGB& left, const FrameRGB& right);

}
