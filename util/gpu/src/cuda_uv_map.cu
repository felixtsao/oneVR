#include "onevr/cuda_uv_map.h"

#include <cmath>
#include <cuda_runtime.h>
#include <iostream>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

namespace onevr::cuda {

static inline void handle_cuda_error(cudaError_t e, const char* what) {
    if (e != cudaSuccess) {
        throw std::runtime_error(std::string(what) + ": " + cudaGetErrorString(e));
    }
}

static __device__ __forceinline__ uint8_t clamp_u8(int v) {
    return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

static __device__ __forceinline__ int idx3(int x, int y, int w) {
    return (y * w + x) * 3;
}

static __device__ __forceinline__ uint8_t brightness_contrast(uint8_t v, float contrast, float brightness) {
    float f = contrast * (float(v) - 128.0f) + 128.0f + brightness;
    return clamp_u8((int)f);
}

__global__ void project_bilinear(const uint8_t* __restrict__ src,
                                 int src_w,
                                 int src_h,
                                 const Uv* __restrict__ lut,
                                 int out_w,
                                 int out_h,
                                 uint8_t* __restrict__ dst) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= out_w || y >= out_h)
        return;

    const Uv uv = lut[y * out_w + x];
    if (!uv.valid) {
        int o = idx3(x, y, out_w);
        dst[o + 0] = 0;
        dst[o + 1] = 0;
        dst[o + 2] = 0;
        return;
    }

    // u,v should be in pixel space, reject outliers
    const float u = uv.u;
    const float v = uv.v;
    if (!isfinite(u) || !isfinite(v))
        return;
    if (u < 0.f || u > (src_w - 1) || v < 0.f || v > (src_h - 1))
        return;

    // Bilinear resample
    const float fx = uv.u;
    const float fy = uv.v;

    int x0 = (int)floorf(fx);
    int y0 = (int)floorf(fy);
    int x1 = min(x0 + 1, src_w - 1);
    int y1 = min(y0 + 1, src_h - 1);

    float tx = fx - x0;
    float ty = fy - y0;

    int i00 = idx3(x0, y0, src_w);
    int i10 = idx3(x1, y0, src_w);
    int i01 = idx3(x0, y1, src_w);
    int i11 = idx3(x1, y1, src_w);

    // Bilinear for each channel
    float w00 = (1.0f - tx) * (1.0f - ty);
    float w10 = tx * (1.0f - ty);
    float w01 = (1.0f - tx) * ty;
    float w11 = tx * ty;

    int o = idx3(x, y, out_w);

    for (int c = 0; c < 3; ++c) {
        float v = w00 * src[i00 + c] + w10 * src[i10 + c] + w01 * src[i01 + c] + w11 * src[i11 + c];
        dst[o + c] = clamp_u8((int)lrintf(v));
    }
}

__global__ void project_bilinear_sbs(const uint8_t* __restrict__ src,
                                     int src_w,
                                     int src_h,
                                     const Uv* __restrict__ lut, // eye_w * eye_h
                                     int eye_w,
                                     int eye_h,
                                     int sbs_w,
                                     int dst_x_offset,
                                     float contrast,
                                     float brightness,
                                     uint8_t* __restrict__ dst) // sbs_w * eye_h * 3
{
    int x = blockIdx.x * blockDim.x + threadIdx.x; // 0..eye_w-1
    int y = blockIdx.y * blockDim.y + threadIdx.y; // 0..eye_h-1
    if (x >= eye_w || y >= eye_h)
        return;

    // LUT INDEX MUST BE EYE SPACE:
    const Uv uv = lut[y * eye_w + x];

    int out_x = x + dst_x_offset;
    int o = (y * sbs_w + out_x) * 3;

    if (!uv.valid) {
        dst[o] = 0;
        dst[o + 1] = 0;
        dst[o + 2] = 0;
        return;
    }

    float fx = uv.u, fy = uv.v;
    if (!isfinite(fx) || !isfinite(fy)) {
        dst[o] = 0;
        dst[o + 1] = 0;
        dst[o + 2] = 0;
        return;
    }
    if (fx < 0.f || fx > (src_w - 1) || fy < 0.f || fy > (src_h - 1)) {
        dst[o] = 0;
        dst[o + 1] = 0;
        dst[o + 2] = 0;
        return;
    }

    int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
    int x1 = min(x0 + 1, src_w - 1);
    int y1 = min(y0 + 1, src_h - 1);

    float tx = fx - x0, ty = fy - y0;
    float w00 = (1.f - tx) * (1.f - ty);
    float w10 = tx * (1.f - ty);
    float w01 = (1.f - tx) * ty;
    float w11 = tx * ty;

    int i00 = (y0 * src_w + x0) * 3;
    int i10 = (y0 * src_w + x1) * 3;
    int i01 = (y1 * src_w + x0) * 3;
    int i11 = (y1 * src_w + x1) * 3;

    for (int c = 0; c < 3; ++c) {
        float vv = w00 * src[i00 + c] + w10 * src[i10 + c] + w01 * src[i01 + c] + w11 * src[i11 + c];
        dst[o + c] = brightness_contrast(clamp_u8((int)lrintf(vv)), contrast, brightness);
    }
}

rgb::Frame project_bilinear(const rgb::Frame& src, const UvMap& lut) {
    const int out_w = lut.width;
    const int out_h = lut.height;

    rgb::Frame dst(out_w, out_h);

    const size_t src_bytes = (size_t)src.width * src.height * 3;
    const size_t dst_bytes = (size_t)dst.width * dst.height * 3;
    const size_t lut_bytes = (size_t)out_w * out_h * sizeof(Uv);

    uint8_t* d_src = nullptr;
    uint8_t* d_dst = nullptr;
    Uv* d_lut = nullptr;

    handle_cuda_error(cudaMalloc(&d_src, src_bytes), "cudaMalloc d_src");
    handle_cuda_error(cudaMalloc(&d_dst, dst_bytes), "cudaMalloc d_dst");
    handle_cuda_error(cudaMalloc(&d_lut, lut_bytes), "cudaMalloc d_lut");

    handle_cuda_error(cudaMemcpy(d_src, src.data.data(), src_bytes, cudaMemcpyHostToDevice), "cudaMemcpy H2D src");
    handle_cuda_error(cudaMemcpy(d_lut, lut.data.data(), lut_bytes, cudaMemcpyHostToDevice), "cudaMemcpy H2D lut");

    dim3 block(16, 16);
    dim3 grid((out_w + block.x - 1) / block.x, (out_h + block.y - 1) / block.y);

    project_bilinear<<<grid, block>>>(d_src, src.width, src.height, d_lut, out_w, out_h, d_dst);

    handle_cuda_error(cudaGetLastError(), "kernel launch");
    handle_cuda_error(cudaDeviceSynchronize(), "cudaDeviceSynchronize");

    handle_cuda_error(cudaMemcpy(dst.data.data(), d_dst, dst_bytes, cudaMemcpyDeviceToHost), "cudaMemcpy D2H dst");

    cudaFree(d_src);
    cudaFree(d_dst);
    cudaFree(d_lut);

    return dst;
}

void project_bilinear(const rgb::Frame& src,
                      uint8_t* d_src,
                      const UvMap& lut,
                      const Uv* d_lut,
                      int lut_x_offset,
                      float contrast,
                      float brightness,
                      uint8_t* target) {
    const int out_w = lut.width;
    const int out_h = lut.height;
    const size_t src_bytes = src.width * src.height * 3;

    handle_cuda_error(cudaMemcpy(d_src, src.data.data(), src_bytes, cudaMemcpyHostToDevice), "H2D src");

    dim3 block(16, 16);
    dim3 grid((out_w + block.x - 1) / block.x, (out_h + block.y - 1) / block.y);

    project_bilinear_sbs<<<grid, block>>>(d_src,
                                          src.width,
                                          src.height,
                                          d_lut,
                                          lut.width,
                                          lut.height,
                                          2 * lut.width,
                                          lut_x_offset,
                                          contrast,
                                          brightness,
                                          target);

    handle_cuda_error(cudaGetLastError(), "warp kernel");
    handle_cuda_error(cudaDeviceSynchronize(), "warp sync");
}

// BT.709
__global__ void rgb_to_nv12_into_hw_kernel(const uint8_t* __restrict__ rgb, // tight: (y*w + x)*3
                                           int w,
                                           int h,
                                           uint8_t* __restrict__ y_plane,
                                           int y_pitch,
                                           uint8_t* __restrict__ uv_plane,
                                           int uv_pitch) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int yy = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || yy >= h)
        return;

    const uint8_t* p = rgb + (yy * w + x) * 3;
    int R = p[0], G = p[1], B = p[2];

    // Y' (limited range)
    int Y = ((47 * R + 157 * G + 16 * B + 128) >> 8) + 16;
    y_plane[yy * y_pitch + x] = clamp_u8(Y);

    // UV once per 2x2 block
    if (((x & 1) == 0) && ((yy & 1) == 0) && (x + 1 < w) && (yy + 1 < h)) {
        int sumR = 0, sumG = 0, sumB = 0;

#pragma unroll
        for (int dy = 0; dy < 2; ++dy) {
#pragma unroll
            for (int dx = 0; dx < 2; ++dx) {
                const uint8_t* q = rgb + ((yy + dy) * w + (x + dx)) * 3;
                sumR += q[0];
                sumG += q[1];
                sumB += q[2];
            }
        }
        int r = (sumR + 2) >> 2;
        int g = (sumG + 2) >> 2;
        int b = (sumB + 2) >> 2;

        int U = ((-26 * r - 87 * g + 112 * b + 128) >> 8) + 128;
        int V = ((112 * r - 102 * g - 10 * b + 128) >> 8) + 128;

        int uv_y = yy >> 1;
        uint8_t* row = uv_plane + uv_y * uv_pitch;
        row[x + 0] = clamp_u8(U);
        row[x + 1] = clamp_u8(V);
    }
}

extern "C" void
rgb_to_nv12_into_hw(const uint8_t* rgb, int w, int h, uint8_t* y_plane, int y_pitch, uint8_t* uv_plane, int uv_pitch) {
    dim3 block(32, 8);
    dim3 grid((w + block.x - 1) / block.x, (h + block.y - 1) / block.y);
    rgb_to_nv12_into_hw_kernel<<<grid, block>>>(rgb, w, h, y_plane, y_pitch, uv_plane, uv_pitch);
}

} // namespace onevr::cuda