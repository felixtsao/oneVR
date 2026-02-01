#include "onevr/image_processing.h"
#include <cstring>
#include <stdexcept>

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
}

namespace onevr {

FrameRGB scale_rgb24(const FrameRGB& in, int out_w, int out_h) {
    if (in.width <= 0 || in.height <= 0 || out_w <= 0 || out_h <= 0)
        throw std::runtime_error("scale_rgb24: invalid dims");
    if (in.stride < in.width * 3)
        throw std::runtime_error("scale_rgb24: invalid stride");

    SwsContext* sws = sws_getContext(
        in.width, in.height, AV_PIX_FMT_RGB24,
        out_w, out_h, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    if (!sws) throw std::runtime_error("scale_rgb24: sws_getContext failed");

    FrameRGB out;
    out.width = out_w;
    out.height = out_h;
    out.stride = out_w * 3;
    out.data.resize(static_cast<size_t>(out.stride * out.height));

    const uint8_t* src_slices[4] = { in.data.data(), nullptr, nullptr, nullptr };
    int src_stride[4] = { in.stride, 0, 0, 0 };

    uint8_t* dst_slices[4] = { out.data.data(), nullptr, nullptr, nullptr };
    int dst_stride[4] = { out.stride, 0, 0, 0 };

    sws_scale(sws, src_slices, src_stride, 0, in.height, dst_slices, dst_stride);
    sws_freeContext(sws);
    return out;
}

FrameRGB sbs_rgb(const onevr::FrameRGB& L, const onevr::FrameRGB& R) {
    if (L.width != R.width || L.height != R.height) {
        throw std::runtime_error("sbs_rgb: L/R size mismatch");
    }
    if (L.stride < L.width * 3 || R.stride < R.width * 3) {
        throw std::runtime_error("sbs_rgb: invalid stride");
    }

    onevr::FrameRGB out;
    out.width = L.width + R.width;
    out.height = L.height;
    out.stride = out.width * 3;
    out.data.resize(static_cast<size_t>(out.stride * out.height));

    for (int y = 0; y < out.height; ++y) {
        const uint8_t* lrow = L.data.data() + y * L.stride;
        const uint8_t* rrow = R.data.data() + y * R.stride;
        uint8_t* orow = out.data.data() + y * out.stride;

        std::memcpy(orow, lrow, static_cast<size_t>(L.width * 3));
        std::memcpy(orow + L.width * 3, rrow, static_cast<size_t>(R.width * 3));
    }

    return out;
}

}  // namespace onevr