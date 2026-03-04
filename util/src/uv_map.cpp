#include "onevr/uv_map.h"

#include <cmath>
#include <stdexcept>

namespace onevr {
static inline int idx(int x, int y, int w) {
    return y * w + x;
}
static inline uint8_t clamp_u8(int v) {
    if (v < 0)
        return 0;
    if (v > 255)
        return 255;
    return static_cast<uint8_t>(v);
}

rgb::Frame project_nearest(const rgb::Frame& in, const UvMap& lut) {
    if (in.width <= 0 || in.height <= 0)
        throw std::runtime_error("warp_nearest: invalid input dims");
    if (in.stride < in.width * 3)
        throw std::runtime_error("warp_nearest: invalid input stride");
    if (lut.width <= 0 || lut.height <= 0)
        throw std::runtime_error("warp_nearest: invalid lut dims");
    if (static_cast<int>(lut.data.size()) != lut.width * lut.height)
        throw std::runtime_error("warp_nearest: lut size mismatch");

    rgb::Frame out;
    out.width = lut.width;
    out.height = lut.height;
    out.stride = out.width * 3;
    out.data.assign(static_cast<size_t>(out.stride) * out.height, 0);

    for (int y = 0; y < out.height; ++y) {
        uint8_t* out_row = out.data.data() + y * out.stride;
        for (int x = 0; x < out.width; ++x) {
            const onevr::Uv& uv = lut.data[onevr::idx(x, y, out.width)];
            uint8_t* px = out_row + x * 3;

            if (!uv.valid) {
                px[0] = px[1] = px[2] = 0;
                continue;
            }

            const int u = static_cast<int>(std::lround(uv.u));
            const int v = static_cast<int>(std::lround(uv.v));

            if (u < 0 || u >= in.width || v < 0 || v >= in.height) {
                px[0] = px[1] = px[2] = 0;
                continue;
            }

            const uint8_t* in_px = in.data.data() + v * in.stride + u * 3;
            px[0] = in_px[0];
            px[1] = in_px[1];
            px[2] = in_px[2];
        }
    }

    return out;
}

rgb::Frame project_bilinear(const rgb::Frame& in, const UvMap& lut) {
    if (in.width <= 0 || in.height <= 0)
        throw std::runtime_error("warp_bilinear: invalid input dims");
    if (in.stride < in.width * 3)
        throw std::runtime_error("warp_bilinear: invalid input stride");
    if (lut.width <= 0 || lut.height <= 0)
        throw std::runtime_error("warp_bilinear: invalid lut dims");
    if (static_cast<int>(lut.data.size()) != lut.width * lut.height)
        throw std::runtime_error("warp_bilinear: lut size mismatch");

    rgb::Frame out;
    out.width = lut.width;
    out.height = lut.height;
    out.stride = out.width * 3;
    out.data.assign(static_cast<size_t>(out.stride) * out.height, 0);

    for (int y = 0; y < out.height; ++y) {
        uint8_t* out_row = out.data.data() + y * out.stride;
        for (int x = 0; x < out.width; ++x) {
            const onevr::Uv& uv = lut.data[onevr::idx(x, y, out.width)];
            uint8_t* px = out_row + x * 3;

            if (!uv.valid) {
                px[0] = px[1] = px[2] = 0;
                continue;
            }

            const float uf = uv.u;
            const float vf = uv.v;

            const int u0 = static_cast<int>(std::floor(uf));
            const int v0 = static_cast<int>(std::floor(vf));
            const int u1 = u0 + 1;
            const int v1 = v0 + 1;

            if (u0 < 0 || v0 < 0 || u1 >= in.width || v1 >= in.height) {
                px[0] = px[1] = px[2] = 0;
                continue;
            }

            const float du = uf - u0;
            const float dv = vf - v0;

            const uint8_t* p00 = in.data.data() + v0 * in.stride + u0 * 3;
            const uint8_t* p10 = in.data.data() + v0 * in.stride + u1 * 3;
            const uint8_t* p01 = in.data.data() + v1 * in.stride + u0 * 3;
            const uint8_t* p11 = in.data.data() + v1 * in.stride + u1 * 3;

            for (int c = 0; c < 3; ++c) {
                const float a = p00[c] * (1.0f - du) + p10[c] * du;
                const float b = p01[c] * (1.0f - du) + p11[c] * du;
                const float val = a * (1.0f - dv) + b * dv;
                px[c] = onevr::clamp_u8(static_cast<int>(std::lround(val)));
            }
        }
    }

    return out;
}

} // namespace onevr