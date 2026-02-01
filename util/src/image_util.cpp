#include "onevr/image_util.h"
#include <cstdio>
#include <stdexcept>

namespace onevr {

void write_ppm(const std::string& path, const FrameRGB& frame) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) throw std::runtime_error("failed to open output file: " + path);

    std::fprintf(f, "P6\n%d %d\n255\n", frame.width, frame.height);
    for (int y = 0; y < frame.height; ++y) {
        const uint8_t* row = frame.data.data() + y * frame.stride;
        std::fwrite(row, 1, static_cast<size_t>(frame.width * 3), f);
    }
    std::fclose(f);
}

}  // namespace onevr
