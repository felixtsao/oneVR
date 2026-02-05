#include "onevr/cuda_uv_map.h"

#include <cuda_runtime.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
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

__global__ void project_bilinear(
    const uint8_t* __restrict__ src,
    int src_w, int src_h,
    const Uv* __restrict__ lut,
    int out_w, int out_h,
    uint8_t* __restrict__ dst)
{
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= out_w || y >= out_h) return;

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
  if (!isfinite(u) || !isfinite(v)) return;
  if (u < 0.f || u > (src_w - 1) || v < 0.f || v > (src_h - 1)) return;

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
    float v =
        w00 * src[i00 + c] +
        w10 * src[i10 + c] +
        w01 * src[i01 + c] +
        w11 * src[i11 + c];
    dst[o + c] = clamp_u8((int)lrintf(v));
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

  handle_cuda_error(cudaMemcpy(d_src, src.data.data(), src_bytes, cudaMemcpyHostToDevice),
            "cudaMemcpy H2D src");
  handle_cuda_error(cudaMemcpy(d_lut, lut.data.data(), lut_bytes, cudaMemcpyHostToDevice),
            "cudaMemcpy H2D lut");

  dim3 block(16, 16);
  dim3 grid((out_w + block.x - 1) / block.x,
            (out_h + block.y - 1) / block.y);

  project_bilinear<<<grid, block>>>(
      d_src, src.width, src.height,
      d_lut, out_w, out_h,
      d_dst);

  handle_cuda_error(cudaGetLastError(), "kernel launch");
  handle_cuda_error(cudaDeviceSynchronize(), "cudaDeviceSynchronize");

  handle_cuda_error(cudaMemcpy(dst.data.data(), d_dst, dst_bytes, cudaMemcpyDeviceToHost),
            "cudaMemcpy D2H dst");

  cudaFree(d_src);
  cudaFree(d_dst);
  cudaFree(d_lut);

  return dst;
}

}  // namespace onevr::cuda