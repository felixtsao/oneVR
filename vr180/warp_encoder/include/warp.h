#pragma once
#include "onevr/frame.h"
#include "onevr/uv_map.h"

#include <cstdint>

namespace onevr::vr180 {

static const float PI = 3.14159265358979323846;

enum class InterpolationMethod { NEAREST_NEIGHBOR, BILINEAR };

struct Camera {
    std::string name;

    // Sensor dimensions (pixels)
    int width = 0;
    int height = 0;

    // Horizontal field of view
    float hfov_degrees = 0.0f;

    // Principal point (defaults to center if <0).
    float cx = -1.0f;
    float cy = -1.0f;

    float fx = 1.0f;
    float fy = 1.0f;

    // Lens distortion correction
    bool lens_distortion = false;
    struct DistortionCoefficients {
        float k1 = 0.0f;
        float k2 = 0.0f;
        float k3 = 0.0f;
        float p1 = 0.0f;
        float p2 = 0.0f;
    } lens_distortion_coefficients;
};

struct WarpSettings {
    // Per-eye output size. Default: 4096x4096 (SBS 8192x4096)
    int eye_width = 4096;
    int eye_height = 4096;

    // Default output angular coverage: +/-90° yaw, +/-90° pitch
    float yaw_half_rad = PI * 0.5f;
    float pitch_half_rad = PI * 0.5f;

    float yaw_offset_degrees = 0.0f;
    float pitch_offset_degrees = 0.0f;
    float roll_offset_degrees = 0.0f;

    InterpolationMethod interpolation_method = InterpolationMethod::BILINEAR;
};

struct WarpGpuMemory {
    uint8_t* d_src = nullptr;
    uint8_t* sbs_composite = nullptr;
    Uv* d_lut = nullptr;

    size_t src_bytes = 0;
    size_t lut_bytes = 0;
    size_t sbs_composite_bytes = 0;
};

// Synthesize look-up table which encodes warp/camera projection characteristics used throughout
// runtime
onevr::UvMap slut(const Camera& cam, const WarpSettings& s);

// Map from camera sensor space to VR180 equirectangular and project back to screen space
onevr::rgb::Frame warp(const onevr::rgb::Frame& in, const onevr::UvMap& lut, InterpolationMethod type);

} // namespace onevr::vr180

namespace onevr::vr180::cuda {

void init_warp_memory(WarpGpuMemory& gpu,
                      size_t src_width,
                      size_t src_height,
                      size_t output_width,
                      size_t output_height,
                      const UvMap& lut);

onevr::rgb::Frame warp(const onevr::rgb::Frame& in, const onevr::UvMap& lut, InterpolationMethod type);
void warp(WarpGpuMemory& gpu_resources,
          const onevr::rgb::Frame& in,
          const onevr::UvMap& lut,
          int lut_x_offset,
          InterpolationMethod type,
          float contrast,
          float brightness,
          uint8_t* target);

} // namespace onevr::vr180::cuda
