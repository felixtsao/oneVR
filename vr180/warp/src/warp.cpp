#include "warp.h"

#include "onevr/cuda_uv_map.h"
#include "onevr/uv_map.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace onevr::vr180 {

static inline int idx(int x, int y, int w) {
    return y * w + x;
}

static inline float deg2rad(float d) {
    return d * static_cast<float>(PI) / 180.0f;
}

onevr::UvMap slut(const Camera& cam, const Vr180WarpSettings& s) {

    if (cam.width <= 0 || cam.height <= 0)
        throw std::runtime_error("create_warp_slut: invalid camera dimensions");
    if (s.eye_width <= 0 || s.eye_height <= 0)
        throw std::runtime_error("create_warp_slut: invalid eye dimensions");
    if (cam.hfov_degrees <= 0.0f || cam.hfov_degrees >= 180.0f)
        throw std::runtime_error("hfov_deg must be in (0, 180)");

    const float hfov_radians = deg2rad(cam.hfov_degrees);
    if (hfov_radians <= 0.0f || hfov_radians >= 3.13f)
        throw std::runtime_error("create_warp_slut: hfov_rad out of range");

    const float cx = (cam.cx >= 0.0f) ? cam.cx : (cam.width * 0.5f);
    const float cy = (cam.cy >= 0.0f) ? cam.cy : (cam.height * 0.5f);

    // Rectilinear focal length from HFOV:
    const float f = (cam.width * 0.5f) / std::tan(hfov_radians * 0.5f);

    onevr::UvMap lut(s.eye_width, s.eye_height);

    for (int y = 0; y < lut.height; ++y) {
        const float ny = ((y + 0.5f) / lut.height) * 2.0f - 1.0f;
        const float phi = ny * s.pitch_half_rad; // pitch
        const float sin_phi = std::sin(phi);
        const float cos_phi = std::cos(phi);

        for (int x = 0; x < lut.width; ++x) {
            const float nx = ((x + 0.5f) / lut.width) * 2.0f - 1.0f;
            const float theta = nx * s.yaw_half_rad; // yaw

            const float sin_theta = std::sin(theta);
            const float cos_theta = std::cos(theta);

            // Spherical direction (right-handed):
            // x right, y up, z forward
            const float dir_x = cos_phi * sin_theta;
            const float dir_y = sin_phi;
            const float dir_z = cos_phi * cos_theta;

            onevr::Uv uv;

            // Only front hemisphere.
            if (dir_z <= 1e-6f) {
                uv.valid = 0;
                lut.at(x, y) = uv;
                continue;
            }

            const float u = f * (dir_x / dir_z) + cx;
            const float v = f * (dir_y / dir_z) + cy;

            if (u >= 0.0f && u <= (cam.width - 1.0f) && v >= 0.0f && v <= (cam.height - 1.0f)) {
                uv.u = u;
                uv.v = v;
                uv.valid = 1;
            } else {
                uv.valid = 0;
            }

            lut.at(x, y) = uv;
        }
    }

    return lut;
}

/*
 * Warp using CPU
 */
onevr::rgb::Frame
warp(const onevr::rgb::Frame& in, const onevr::UvMap& lut, InterpolationMethod interp) {
    switch (interp) {
        case InterpolationMethod::NEAREST_NEIGHBOR:
            return onevr::project_nearest(in, lut);
        case InterpolationMethod::BILINEAR:
            return onevr::project_bilinear(in, lut);
    }
    return {};
}

} // namespace onevr::vr180

namespace onevr::vr180::cuda {

// Warp using GPU but does H2D io/copies back to CPU
onevr::rgb::Frame
warp(const onevr::rgb::Frame& in, const onevr::UvMap& lut, InterpolationMethod interp) {
    return onevr::cuda::project_bilinear(in, lut);
}

// Warp in-place on GPU memory
void warp(const onevr::rgb::Frame& in,
          const onevr::UvMap& lut,
          int lut_x_offset,
          InterpolationMethod interp,
          uint8_t* target) {
    onevr::cuda::project_bilinear(in, lut, lut_x_offset, target);
}

} // namespace onevr::vr180::cuda
