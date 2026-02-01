#include <iostream>

extern "C" {
#include <libavutil/avutil.h>
}

#include "onevr/ffmpeg_util.h"

int main() {
    std::cout << "FFmpeg version: " << av_version_info() << "\n";

    // Sanity check: call into shared lib
    std::cout << "FFmpeg error example: "
              << onevr::ff_err(AVERROR_EOF) << "\n";

    return 0;
}
