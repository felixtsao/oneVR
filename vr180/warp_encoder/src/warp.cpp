#include "warp.h"

#include "onevr/cuda_uv_map.h"
#include "onevr/uv_map.h"

#include <cmath>
#include <cuda_runtime.h>
#include <iostream>
#include <stdexcept>

namespace onevr::vr180 {

static inline int idx(int x, int y, int w) {
    return y * w + x;
}

static inline float deg2rad(float d) {
    return d * static_cast<float>(PI) / 180.0f;
}

static inline void
distort_brown_conrady(float x, float y, const onevr::vr180::Camera::DistortionCoefficients& d, float* xd, float* yd) {
    float r2 = x * x + y * y;
    float r4 = r2 * r2;
    float r6 = r4 * r2;

    float radial = 1.0f + d.k1 * r2 + d.k2 * r4 + d.k3 * r6;

    float x_tan = 2.0f * d.p1 * x * y + d.p2 * (r2 + 2.0f * x * x);
    float y_tan = d.p1 * (r2 + 2.0f * y * y) + 2.0f * d.p2 * x * y;

    *xd = x * radial + x_tan;
    *yd = y * radial + y_tan;
}

onevr::UvMap slut(const Camera& cam, const WarpSettings& s) {
    if (cam.width <= 0 || cam.height <= 0) {
        throw std::runtime_error("slut: invalid camera dimensions");
    }
    if (s.eye_width <= 0 || s.eye_height <= 0) {
        throw std::runtime_error("slut: invalid eye dimensions");
    }
    if (s.yaw_half_rad <= 0.f || s.pitch_half_rad <= 0.f) {
        throw std::runtime_error("slut: yaw_half_rad/pitch_half_rad must be > 0");
    }

    // Intrinsics (prefer provided fx/fy/cx/cy; fallback to HFOV-derived f)
    float fx = cam.fx;
    float fy = cam.fy;
    float cx = (cam.cx >= 0.f) ? cam.cx : (cam.width * 0.5f);
    float cy = (cam.cy >= 0.f) ? cam.cy : (cam.height * 0.5f);

    if (cam.hfov_degrees >= 0.0 || fx <= 0.f || fy <= 0.f) {
        if (cam.hfov_degrees <= 0.f || cam.hfov_degrees >= 180.f) {
            throw std::runtime_error("slut: need fx/fy or hfov_degrees in (0,180)");
        }
        const float hfov_rad = deg2rad(cam.hfov_degrees);
        const float f = (cam.width * 0.5f) / std::tan(hfov_rad * 0.5f);
        fx = f;
        fy = f;
    }

    auto rotate_yaw_pitch_roll = [](float& x, float& y, float& z, float yaw, float pitch, float roll) {
        // Apply yaw (Y), pitch (X), roll (Z) in that order.
        // If the sign feels inverted in practice, flip the offsets (common convention issue).

        // yaw around +Y
        {
            float cy = std::cos(yaw), sy = std::sin(yaw);
            float nx = cy * x + sy * z;
            float nz = -sy * x + cy * z;
            x = nx;
            z = nz;
        }
        // pitch around +X
        {
            float cp = std::cos(pitch), sp = std::sin(pitch);
            float ny = cp * y - sp * z;
            float nz = sp * y + cp * z;
            y = ny;
            z = nz;
        }
        // roll around +Z
        {
            float cr = std::cos(roll), sr = std::sin(roll);
            float nx = cr * x - sr * y;
            float ny = sr * x + cr * y;
            x = nx;
            y = ny;
        }
    };

    onevr::UvMap lut(s.eye_width, s.eye_height);

    for (int y = 0; y < lut.height; ++y) {
        // v01 in [0..1], top=0
        const float v01 = (y + 0.5f) / (float)lut.height;
        // latitude: +pitch at top, -pitch at bottom
        const float lat = (0.5f - v01) * (2.0f * s.pitch_half_rad);

        const float sin_lat = std::sin(lat);
        const float cos_lat = std::cos(lat);

        for (int x = 0; x < lut.width; ++x) {
            const float u01 = (x + 0.5f) / (float)lut.width;
            // longitude: left=-yaw, right=+yaw
            const float lon = (u01 - 0.5f) * (2.0f * s.yaw_half_rad);

            // right-handed: X left, Y down, Z forward
            float dir_x = cos_lat * std::sin(lon);
            float dir_y = -sin_lat;
            float dir_z = cos_lat * std::cos(lon);

            // Apply user offsets (center tweaks)
            rotate_yaw_pitch_roll(dir_x,
                                  dir_y,
                                  dir_z,
                                  deg2rad(s.yaw_offset_degrees),
                                  deg2rad(s.pitch_offset_degrees),
                                  deg2rad(s.roll_offset_degrees));

            onevr::Uv uv{};
            uv.valid = 0;

            // Only rays in front of the camera (pinhole model)
            if (dir_z <= 1e-6f) {
                lut.at(x, y) = uv;
                continue;
            }

            // Project to normalized pinhole plane
            const float xn = dir_x / dir_z;
            const float yn = dir_y / dir_z;

            // Cull outside of nominal camera lens fov
            float x_min = (0.0f - cx) / fx;
            float x_max = ((cam.width - 1.0f) - cx) / fx;
            float y_min = (0.0f - cy) / fy;
            float y_max = ((cam.height - 1.0f) - cy) / fy;

            float r_max =
                std::min(std::min(std::fabs(x_min), std::fabs(x_max)), std::min(std::fabs(y_min), std::fabs(y_max)));
            float r2_max = r_max * r_max;

            float r2 = xn * xn + yn * yn;
            if (r2 > r2_max) {
                uv.valid = 0;
                lut.at(x, y) = uv;
                continue;
            }

            // Optional lens distortion (Brown–Conrady as you already use it)
            float xd = xn;
            float yd = yn;
            if (cam.lens_distortion) {
                distort_brown_conrady(xn, yn, cam.lens_distortion_coefficients, &xd, &yd);
            }

            // Intrinsics to pixel coords
            const float u = fx * xd + cx;
            const float v = fy * yd + cy;

            if (u >= 0.f && u <= (cam.width - 1.f) && v >= 0.f && v <= (cam.height - 1.f)) {
                uv.u = u;
                uv.v = v;
                uv.valid = 1;
            }

            lut.at(x, y) = uv;
        }
    }

    return lut;
}

/*
 * Warp using CPU
 */
onevr::rgb::Frame warp(const onevr::rgb::Frame& in, const onevr::UvMap& lut, InterpolationMethod interp) {
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

void init_warp_memory(WarpGpuMemory& gpu,
                      size_t src_width,
                      size_t src_height,
                      size_t output_width,
                      size_t output_height,
                      const UvMap& lut) {

    gpu.lut_bytes = lut.width * lut.height * sizeof(Uv);
    gpu.src_bytes = src_width * src_height * 3;
    gpu.sbs_composite_bytes = output_width * output_height * 3; // 3 channel RGB

    cudaMalloc(&gpu.d_lut, gpu.lut_bytes);
    cudaMemcpy(gpu.d_lut, lut.data.data(), gpu.lut_bytes, cudaMemcpyHostToDevice);

    cudaMalloc(&gpu.d_src, gpu.src_bytes);
    cudaMalloc(&gpu.sbs_composite, gpu.sbs_composite_bytes);
}

// Warp using GPU but does H2D io/copies back to CPU
onevr::rgb::Frame warp(const onevr::rgb::Frame& in, const onevr::UvMap& lut, InterpolationMethod interp) {
    return onevr::cuda::project_bilinear(in, lut);
}

// Warp in-place on GPU memory
void warp(WarpGpuMemory& gpu_memory,
          const onevr::rgb::Frame& in,
          const onevr::UvMap& lut,
          int lut_x_offset,
          InterpolationMethod interp,
          float contrast,
          float brightness,
          uint8_t* target) {
    onevr::cuda::project_bilinear(
        in, gpu_memory.d_src, lut, gpu_memory.d_lut, lut_x_offset, contrast, brightness, target);
}

} // namespace onevr::vr180::cuda
