#include "onevr/image_processing.h"

#include <cstring>
#include <stdexcept>

extern "C" {
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

namespace onevr {

rgb::Frame cat_sbs(const onevr::rgb::Frame& L, const onevr::rgb::Frame& R) {
    if (L.width != R.width || L.height != R.height) {
        throw std::runtime_error("sbs_rgb: L/R size mismatch");
    }
    if (L.stride < L.width * 3 || R.stride < R.width * 3) {
        throw std::runtime_error("sbs_rgb: invalid stride");
    }

    onevr::rgb::Frame out;
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

} // namespace onevr