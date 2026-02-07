#pragma once
#include "onevr/frame.h"
#include "onevr/uv_map.h"

#include <cstdint>

namespace onevr::vr180 {

static const float PI = 3.14159265358979323846;

enum class InterpolationMethod { NEAREST_NEIGHBOR, BILINEAR };

struct Camera {
    // Sensor dimensions (pixels)
    int width = 0;
    int height = 0;

    // Horizontal field of view
    float hfov_degrees = 0.0f;

    // Principal point (defaults to center if <0).
    float cx = -1.0f;
    float cy = -1.0f;
};

struct Vr180WarpSettings {
    // Per-eye output size. Default: 4096x4096 (SBS 8192x4096)
    int eye_width = 4096;
    int eye_height = 4096;

    // Default output angular coverage: +/-90° yaw, +/-90° pitch
    float yaw_half_rad = PI * 0.5f;
    float pitch_half_rad = PI * 0.5f;

    InterpolationMethod interpolation_method = InterpolationMethod::BILINEAR;
};

// Synthesize look-up table which encodes warp/camera projection characteristics used throughout
// runtime
onevr::UvMap slut(const Camera& cam, const Vr180WarpSettings& s);

// Map from camera sensor space to VR180 equirectangular and project back to screen space
onevr::rgb::Frame
warp(const onevr::rgb::Frame& in, const onevr::UvMap& lut, InterpolationMethod type);

} // namespace onevr::vr180

namespace onevr::vr180::cuda {

onevr::rgb::Frame
warp(const onevr::rgb::Frame& in, const onevr::UvMap& lut, InterpolationMethod type);
void warp(const onevr::rgb::Frame& in,
          const onevr::UvMap& lut,
          int lut_x_offset,
          InterpolationMethod type,
          uint8_t* target);

} // namespace onevr::vr180::cuda
